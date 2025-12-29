#pragma once
#include <stdint.h>
#include <stddef.h>

// flags for page table entries
#define P_PRESENT 0x001u
#define P_RW      0x002u
#define P_USER    0x004u

#define PDE_COUNT 1024u
#define PTE_COUNT 1024u

typedef struct page_directory {
    uint32_t* pd_phys;   // physical address of page directory
    uint32_t* pd_virt;   // virtual pointer to same thing (identity mapped for now)
} page_directory_t;

void paging_init_identity(void);

// new:
int  paging_map(uint32_t vaddr, uint32_t paddr, uint32_t flags);
int  paging_unmap(uint32_t vaddr);
uint32_t paging_translate(uint32_t vaddr);
int  paging_alloc_map(uint32_t vaddr, uint32_t flags);

page_directory_t paging_kernel_directory(void);
void paging_switch_directory(page_directory_t dir);

int paging_map_in(page_directory_t dir, uint32_t vaddr, uint32_t paddr, uint32_t flags);
int paging_alloc_map_in(page_directory_t dir, uint32_t vaddr, uint32_t flags);
uint32_t paging_translate_in(page_directory_t dir, uint32_t vaddr);

page_directory_t paging_clone_directory(page_directory_t src);
