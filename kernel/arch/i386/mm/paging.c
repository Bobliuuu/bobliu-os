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

static uint32_t* get_or_alloc_pt(uint32_t vaddr, int make, uint32_t pde_flags) {
    uint32_t pdi = pde_index(vaddr);
    uint32_t pde = g_pd[pdi];

    if (pde & P_PRESENT) {
        // If caller needs user access, upgrade PDE to include P_USER
        if (pde_flags & P_USER) {
            g_pd[pdi] |= P_USER;
        }
        return (uint32_t*)(pde & 0xFFFFF000u); // identity-mapped
    }

    if (!make) return 0;

    uint32_t* pt = alloc_table();
    g_pd[pdi] = ((uint32_t)pt & 0xFFFFF000u) | P_PRESENT | P_RW | (pde_flags & P_USER);
    return pt;
}

int paging_map(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    vaddr &= 0xFFFFF000u;
    paddr &= 0xFFFFF000u;

    // pass flags so PDE can inherit P_USER when needed
    uint32_t* pt = get_or_alloc_pt(vaddr, 1, flags);
    if (!pt) return -1;

    pt[pte_index(vaddr)] = paddr | (flags | P_PRESENT);
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

page_directory_t paging_kernel_directory(void) {
    page_directory_t d;
    d.pd_phys = (uint32_t*)g_pd;
    d.pd_virt = (uint32_t*)g_pd; // identity-mapped for now
    return d;
}

void paging_switch_directory(page_directory_t dir) {
    g_pd = dir.pd_virt;
    asm volatile("mov %0, %%cr3" :: "r"((uint32_t)dir.pd_phys) : "memory");
}

static uint32_t* get_or_alloc_pt_in(page_directory_t dir, uint32_t vaddr, uint32_t flags, int make) {
    uint32_t pdi = pde_index(vaddr);
    uint32_t pde = dir.pd_virt[pdi];

    if (pde & P_PRESENT) {
        if ((flags & P_USER) && !(pde & P_USER)) {
            dir.pd_virt[pdi] |= P_USER;
        }
        return (uint32_t*)(pde & 0xFFFFF000u); // identity mapped PT
    }

    if (!make) return 0;

    uint32_t* pt = alloc_table(); // returns identity-mapped pointer to new PT

    uint32_t pde_flags = P_PRESENT | P_RW;
    if (flags & P_USER) pde_flags |= P_USER;

    dir.pd_virt[pdi] = ((uint32_t)pt & 0xFFFFF000u) | pde_flags;
    return pt;
}

int paging_map_in(page_directory_t dir, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    vaddr &= 0xFFFFF000u;
    paddr &= 0xFFFFF000u;

    uint32_t* pt = get_or_alloc_pt_in(dir, vaddr, flags, 1);
    if (!pt) return -1;

    uint32_t pti = pte_index(vaddr);
    pt[pti] = paddr | flags | P_PRESENT;

    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}

int paging_alloc_map_in(page_directory_t dir, uint32_t vaddr, uint32_t flags) {
    uint32_t p = (uint32_t)pmm_alloc_frame();
    if (!p) return -1;
    return paging_map_in(dir, vaddr, p, flags);
}

static void memcpy32(void* dst, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static void memset32(void* dst, uint8_t v, uint32_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = v;
}

page_directory_t paging_clone_directory(page_directory_t src) {
    // allocate new PD
    uint32_t* new_pd = alloc_table();          // 4KB, zeroed, identity-mapped
    page_directory_t out = { .pd_phys = new_pd, .pd_virt = new_pd };

    // copy PDEs
    for (uint32_t pdi = 0; pdi < 1024; pdi++) {
        uint32_t pde = src.pd_virt[pdi];
        if (!(pde & P_PRESENT)) {
            out.pd_virt[pdi] = 0;
            continue;
        }

        // share kernel identity region (your paging_init_identity mapped PDE 0..3)
        if (pdi < 4) {
            out.pd_virt[pdi] = pde;
            continue;
        }

        // deep copy user PT
        uint32_t* src_pt = (uint32_t*)(pde & 0xFFFFF000u);
        uint32_t* new_pt = alloc_table();

        // carry PDE flags but update PT phys
        uint32_t pde_flags = pde & 0xFFF;
        out.pd_virt[pdi] = ((uint32_t)new_pt & 0xFFFFF000u) | pde_flags;

        for (uint32_t pti = 0; pti < 1024; pti++) {
            uint32_t pte = src_pt[pti];
            if (!(pte & P_PRESENT)) {
                new_pt[pti] = 0;
                continue;
            }

            uint32_t src_frame = pte & 0xFFFFF000u;
            uint32_t pte_flags = pte & 0xFFF;

            // allocate new frame
            uint32_t new_frame = (uint32_t)pmm_alloc_frame();
            if (!new_frame) {
                // OOM: in a real kernel you'd free what you allocated.
                // for now: panic
                extern void panic(const char*);
                panic("paging_clone_directory: OOM");
            }

            // copy contents: identity-map frames in your kernel (0..16MB is mapped)
            // IMPORTANT: this requires src_frame to be identity-mapped
            // For now it is, because you're allocating low memory frames.
            memcpy32((void*)new_frame, (void*)src_frame, 4096);

            new_pt[pti] = new_frame | pte_flags;
        }
    }

    return out;
}
