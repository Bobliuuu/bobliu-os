#pragma once
#include <stdint.h>
#include <stddef.h>

// flags for page table entries
#define P_PRESENT 0x001u
#define P_RW      0x002u
#define P_USER    0x004u

#define PAGE_SIZE 0x1000u
#define PDE_COUNT 1024u
#define PTE_COUNT 1024u

#define KERNEL_PDE_START 0
#define KERNEL_ID_MAP_MB 256u
#define KERNEL_PDE_END   (KERNEL_ID_MAP_MB / 4u)  // 4MB per PDE

/* user address space lives above the identity-mapped kernel region so every
   process can own its page tables (kernel PDEs are shared, user PDEs private) */
#define USER_LO         0x10000000u
#define USER_HI         0x40000000u
#define USER_STACK_TOP  USER_HI
#define USER_STACK_PAGES 8u

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

uint32_t* paging_current_pd_virt(void);

/* switch cr3 only; g_pd (kernel dir used by paging_map) is left alone */
void paging_load(page_directory_t dir);
/* free every user frame + page table (and the directory itself if free_pd) */
void paging_free_user(page_directory_t dir, int free_pd);
/* deep copy of src's user pages into a fresh directory (fork); pd_phys==0 on OOM */
page_directory_t paging_clone_user(page_directory_t src);