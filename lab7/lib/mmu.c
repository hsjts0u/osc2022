#include "mmu.h"
#include "mm.h"
#include "mini_uart.h"

void map_page(struct task_struct *task, unsigned long va, unsigned long page) {
    //printf("===================\n");
    //printf("map virtual address 0x%x to phys address 0x%x\n", va, page);
    unsigned long pgd;
    if (!task->mm.pgd) {
        pgd = (unsigned long)malloc(PAGE_SIZE);
        //printf("pgd not found, created at phys address 0x%x\n", pgd);
        memzero(pgd+VA_START, PAGE_SIZE);
        task->mm.pgd = pgd;
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = task->mm.pgd;
    }
    pgd = task->mm.pgd;
    //printf("pgd at 0x%x\n", pgd);
    int new_table;
    unsigned long pud = map_table((unsigned long *)(pgd + VA_START), PGD_SHIFT, va, &new_table);
    //printf("pud at 0x%x\n", pud);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pud;
    }
    unsigned long pmd = map_table((unsigned long *)(pud + VA_START), PUD_SHIFT, va, &new_table);
    //printf("pmd at 0x%x\n", pmd);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pmd;
    }
    unsigned long pte = map_table((unsigned long *)(pmd + VA_START), PMD_SHIFT, va, &new_table);
    //printf("pte at 0x%x\n", pte);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pte;
    }
    map_table_entry((unsigned long *)(pte + VA_START), va, page);
    struct user_page p = {page, va};
    task->mm.user_pages[task->mm.user_pages_count++] = p;
    //printf("user pages count %d\n", task->mm.user_pages_count-1);
}

unsigned long map_table(unsigned long *table, unsigned long shift, unsigned long va, int* new_table) {
    unsigned long index = va >> shift;
    index = index & (512 - 1);
    if (!table[index]) {
        *new_table = 1;
        unsigned long next_level_table = (unsigned long)malloc(PAGE_SIZE);
        memzero(next_level_table+VA_START, PAGE_SIZE);
        unsigned long entry = next_level_table | PD_TABLE;
        table[index] = entry;
        return next_level_table;
    } else {
        *new_table = 0;
    }
    return table[index] & PAGE_MASK;
}

void map_table_entry(unsigned long *pte, unsigned long va, unsigned long pa) {
    unsigned long index = va >> 12;
    index = index & (512 - 1);
    unsigned long entry = pa | PTE_USR_RW_PTE;
    pte[index] = entry;
    //printf("0x%x[%d] contains 0x%x\n", pte, index, entry);
}

unsigned long va2phys(unsigned long va) {
    printf("translate va: 0x%x\n", va);
    unsigned long pgd;
    asm volatile("mrs %0, ttbr0_el1\n\t": "=r" (pgd) :: "memory");
    unsigned long pud = ((unsigned long*)(pgd+VA_START))[va>>PGD_SHIFT&(512-1)]&PAGE_MASK;
    unsigned long pmd = ((unsigned long*)(pud+VA_START))[va>>PUD_SHIFT&(512-1)]&PAGE_MASK;
    unsigned long pte = ((unsigned long*)(pmd+VA_START))[va>>PMD_SHIFT&(512-1)]&PAGE_MASK;
    unsigned long phys = ((unsigned long*)(pte+VA_START))[va>>12&(512-1)]&PAGE_MASK;
    printf("0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", pgd, pud, pmd, pte, phys);
    return phys;
}
