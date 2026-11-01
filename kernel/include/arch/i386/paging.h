#pragma once
#include <stdint.h>
#include <stddef.h>

// flags for page table entries
#define P_PRESENT 0x001u
#define P_RW      0x002u
#define P_USER    0x004u
#define P_COW     0x200u   /* software bit (PTE avl): write-protected because shared after fork */

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
#define USER_HEAP_BASE  0x20000000u   /* brk() area: [HEAP_BASE, HEAP_MAX) */
#define USER_HEAP_MAX   0x30000000u

/* per-process kernel stacks live in their own shared region, each with an
   unmapped guard page below it (overflow faults instead of corrupting the heap) */
#define KSTACK_BASE     0xE0000000u
#define KSTACK_PDE      (KSTACK_BASE >> 22)
#define KSTACK_BYTES    16384u
#define KSTACK_SLOTS    64u

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
/* drop one page from dir and free its frame (no-op if not mapped) */
void paging_unmap_in(page_directory_t dir, uint32_t vaddr);

page_directory_t paging_clone_directory(page_directory_t src);

uint32_t* paging_current_pd_virt(void);

/* returns the lowest usable address of the stack (top = ret + KSTACK_BYTES), 0 on OOM */
uint32_t kstack_alloc(uint32_t slot);
void     kstack_free(uint32_t slot);

/* switch cr3 only; g_pd (kernel dir used by paging_map) is left alone */
void paging_load(page_directory_t dir);
/* free every user frame + page table (and the directory itself if free_pd) */
void paging_free_user(page_directory_t dir, int free_pd);
/* fork: child shares every user frame copy-on-write with src (src must be the loaded directory) */
page_directory_t paging_clone_user(page_directory_t src);
/* write fault on a cow page in the current directory: 1 if fixed up, 0 if not a cow fault / OOM */
int paging_handle_cow(uint32_t vaddr);