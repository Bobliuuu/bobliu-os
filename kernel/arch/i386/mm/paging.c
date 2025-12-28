#include <stdint.h>
#include <arch/i386/paging.h>
#include <arch/i386/pmm.h>

extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);

#define PAGE_SIZE 4096u
#define PDE_COUNT 1024u
#define PTE_COUNT 1024u

#define P_PRESENT 0x001u
#define P_RW      0x002u

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
