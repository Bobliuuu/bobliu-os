#include <stdint.h>
#include <arch/i386/paging.h>
#include <arch/i386/pmm.h>

extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);

static uint32_t* g_pd = 0;

static inline void write_cr3(uint32_t phys) {
    asm volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
}

static inline uint32_t read_cr0(void) {
    uint32_t v;
    asm volatile("mov %%cr0, %0" : "=r"(v));
    return v;
}

static inline void write_cr0(uint32_t v) {
    asm volatile("mov %0, %%cr0" :: "r"(v) : "memory");
}

static uint32_t* alloc_table(void) {
    uintptr_t phys = pmm_alloc_frame();
    if (!phys) {
        vga_print("paging: OOM\n");
        for(;;) asm volatile("hlt");
    }
    // identity-mapped for now
    uint32_t* t = (uint32_t*)phys;
    for (uint32_t i = 0; i < PTE_COUNT; i++) t[i] = 0;
    return t;
}

void paging_init_identity(void) {
    vga_print("paging: build tables\n");

    g_pd = alloc_table();

    // Identity-map 0..16MB => 4 page tables (PDE 0..3)
    for (uint32_t pde = 0; pde < 4; pde++) {
        uint32_t* pt = alloc_table();

        for (uint32_t pte = 0; pte < PTE_COUNT; pte++) {
            uint32_t addr = (pde * 0x400000u) + (pte * PAGE_SIZE);
            pt[pte] = (addr & 0xFFFFF000u) | P_PRESENT | P_RW;
        }

        g_pd[pde] = ((uint32_t)pt & 0xFFFFF000u) | P_PRESENT | P_RW;
    }

    vga_print("paging: enable cr3=");
    vga_print_hex((uint32_t)g_pd);
    vga_print("\n");

    write_cr3((uint32_t)g_pd);

    uint32_t cr0 = read_cr0();
    cr0 |= 0x80000000u; // PG
    write_cr0(cr0);

    vga_print("paging: enabled\n");
}

static inline uint32_t pde_index(uint32_t v) { return (v >> 22) & 0x3FF; }
static inline uint32_t pte_index(uint32_t v) { return (v >> 12) & 0x3FF; }

static uint32_t* get_or_alloc_pt(uint32_t vaddr, uint32_t flags, int make) {
    uint32_t pdi = pde_index(vaddr);
    uint32_t pde = g_pd[pdi];

    if (pde & P_PRESENT) {
        // Upgrade existing PDE if mapping user pages
        if ((flags & P_USER) && !(pde & P_USER)) {
            g_pd[pdi] |= P_USER;
        }
        return (uint32_t*)(pde & 0xFFFFF000u);
    }

    if (!make) return 0;

    uint32_t* pt = alloc_table();

    uint32_t pde_flags = P_PRESENT | P_RW;
    if (flags & P_USER) pde_flags |= P_USER;

    g_pd[pdi] = ((uint32_t)pt & 0xFFFFF000u) | pde_flags;
    return pt;
}

int paging_map(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    vaddr &= 0xFFFFF000u;
    paddr &= 0xFFFFF000u;

    uint32_t* pt = get_or_alloc_pt(vaddr, flags, 1);
    if (!pt) return -1;

    uint32_t pti = pte_index(vaddr);
    pt[pti] = paddr | (flags | P_PRESENT);

    // flush this page (or reload cr3)
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}

int paging_unmap(uint32_t vaddr) {
    vaddr &= 0xFFFFF000u;
    uint32_t* pt = get_or_alloc_pt(vaddr, 0, 0);
    if (!pt) return -1;

    uint32_t pti = pte_index(vaddr);
    pt[pti] = 0;
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}

uint32_t paging_translate(uint32_t vaddr) {
    uint32_t pdi = pde_index(vaddr);
    uint32_t pti = pte_index(vaddr);

    uint32_t pde = g_pd[pdi];
    if (!(pde & P_PRESENT)) return 0;

    uint32_t* pt = (uint32_t*)(pde & 0xFFFFF000u);
    uint32_t pte = pt[pti];
    if (!(pte & P_PRESENT)) return 0;

    return (pte & 0xFFFFF000u) | (vaddr & 0xFFF);
}

int paging_alloc_map(uint32_t vaddr, uint32_t flags) {
    uint32_t p = (uint32_t)pmm_alloc_frame();
    if (!p) return -1;
    if (paging_map(vaddr, p, flags) < 0) return -1;
    return 0;
}
