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

    // identity-map 0..256MB (KERNEL_PDE_END page tables)
    for (uint32_t pde = 0; pde < KERNEL_PDE_END; pde++) {
        uint32_t* pt = alloc_table();
        for (uint32_t pte = 0; pte < PTE_COUNT; pte++) {
            uint32_t addr = (pde * 0x400000u) + (pte * PAGE_SIZE);
            pt[pte] = (addr & 0xFFFFF000u) | P_PRESENT | P_RW;
        }
        g_pd[pde] = ((uint32_t)pt & 0xFFFFF000u) | P_PRESENT | P_RW;
    }

    // page table for the kernel stack region; shared by all directories
    g_pd[KSTACK_PDE] = ((uint32_t)alloc_table() & 0xFFFFF000u) | P_PRESENT | P_RW;

    vga_print("paging: enable cr3=");
    vga_print_hex((uint32_t)g_pd);
    vga_print("\n");

    write_cr3((uint32_t)g_pd);

    uint32_t cr0 = read_cr0();
    cr0 |= 0x80000000u; // PG
    cr0 |= 0x00010000u; // WP: ring 0 honours read-only pages too (needed for cow)
    write_cr0(cr0);

    vga_print("paging: enabled\n");
}

static inline uint32_t pde_index(uint32_t v) { return (v >> 22) & 0x3FF; }
static inline uint32_t pte_index(uint32_t v) { return (v >> 12) & 0x3FF; }

// Upgrade an existing PDE to be at least as permissive as needed for this mapping.
// For ring3 access, BOTH PDE and PTE must have P_USER; for writes BOTH must have P_RW.
static inline void pde_upgrade(uint32_t* pd, uint32_t pdi, uint32_t need_flags) {
    uint32_t want = 0;
    if (need_flags & P_USER) want |= P_USER;
    if (need_flags & P_RW)   want |= P_RW;

    if (want) {
        uint32_t pde = pd[pdi];
        if ((pde & want) != want) {
            pd[pdi] = pde | want;
        }
    }
}

// Kernel current directory path (uses global g_pd)
static uint32_t* get_or_alloc_pt(uint32_t vaddr, int make, uint32_t need_flags) {
    uint32_t pdi = pde_index(vaddr);
    uint32_t pde = g_pd[pdi];

    if (pde & P_PRESENT) {
        // If caller needs user/rw, PDE must allow it too
        pde_upgrade(g_pd, pdi, need_flags);
        return (uint32_t*)(pde & 0xFFFFF000u); // identity-mapped PT
    }

    if (!make) return 0;

    uint32_t* pt = alloc_table(); // identity-mapped, zeroed

    uint32_t pde_flags = P_PRESENT;
    if (need_flags & P_RW)   pde_flags |= P_RW;
    if (need_flags & P_USER) pde_flags |= P_USER;

    g_pd[pdi] = ((uint32_t)pt & 0xFFFFF000u) | pde_flags;
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
    asm volatile("mov %0, %%cr3" :: "r"((uint32_t)dir.pd_phys) : "memory");
}

void paging_load(page_directory_t dir) {
    asm volatile("mov %0, %%cr3" :: "r"((uint32_t)dir.pd_phys) : "memory");
}

uint32_t paging_translate_in(page_directory_t dir, uint32_t vaddr) {
    uint32_t pde = dir.pd_virt[pde_index(vaddr)];
    if (!(pde & P_PRESENT)) return 0;
    uint32_t* pt = (uint32_t*)(pde & 0xFFFFF000u);
    uint32_t pte = pt[pte_index(vaddr)];
    if (!(pte & P_PRESENT)) return 0;
    return (pte & 0xFFFFF000u) | (vaddr & 0xFFF);
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

void paging_unmap_in(page_directory_t dir, uint32_t vaddr) {
    uint32_t pde = dir.pd_virt[pde_index(vaddr)];
    if (!(pde & P_PRESENT)) return;
    uint32_t* pt = (uint32_t*)(pde & 0xFFFFF000u);
    uint32_t pte = pt[pte_index(vaddr)];
    if (!(pte & P_PRESENT)) return;
    pt[pte_index(vaddr)] = 0;
    pmm_free_frame(pte & 0xFFFFF000u);
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
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
    uint32_t* new_pd = alloc_table();
    page_directory_t out = { .pd_phys = new_pd, .pd_virt = new_pd };

    for (uint32_t pdi = 0; pdi < KERNEL_PDE_END; pdi++) {
        out.pd_virt[pdi] = src.pd_virt[pdi];
    }
    for (uint32_t pdi = KERNEL_PDE_END; pdi < 1024; pdi++) {
        out.pd_virt[pdi] = 0;
    }
    out.pd_virt[KSTACK_PDE] = src.pd_virt[KSTACK_PDE];
    return out;
}

uint32_t* paging_current_pd_virt(void) {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return (uint32_t*)cr3;   // identity mapped
}

void paging_free_user(page_directory_t dir, int free_pd) {
    for (uint32_t pdi = KERNEL_PDE_END; pdi < PDE_COUNT; pdi++) {
        if (pdi == KSTACK_PDE) continue;      // kernel stacks, shared
        uint32_t pde = dir.pd_virt[pdi];
        if (!(pde & P_PRESENT)) continue;
        uint32_t* pt = (uint32_t*)(pde & 0xFFFFF000u);
        for (uint32_t i = 0; i < PTE_COUNT; i++) {
            if (pt[i] & P_PRESENT) pmm_free_frame(pt[i] & 0xFFFFF000u);
        }
        pmm_free_frame((uintptr_t)pt);
        dir.pd_virt[pdi] = 0;
    }
    if (free_pd) pmm_free_frame((uintptr_t)dir.pd_virt);
}

page_directory_t paging_clone_user(page_directory_t src) {
    page_directory_t out = paging_clone_directory(src);

    for (uint32_t pdi = KERNEL_PDE_END; pdi < PDE_COUNT; pdi++) {
        if (pdi == KSTACK_PDE) continue;
        uint32_t pde = src.pd_virt[pdi];
        if (!(pde & P_PRESENT)) continue;
        uint32_t* spt = (uint32_t*)(pde & 0xFFFFF000u);
        uint32_t* dpt = alloc_table();
        out.pd_virt[pdi] = ((uint32_t)dpt) | (pde & 0xFFFu);   // table now owned by out

        for (uint32_t i = 0; i < PTE_COUNT; i++) {
            uint32_t pte = spt[i];
            if (!(pte & P_PRESENT)) continue;

            // writable pages become read-only+cow in BOTH address spaces
            if (pte & P_RW) {
                pte = (pte & ~P_RW) | P_COW;
                spt[i] = pte;
            }
            dpt[i] = pte;
            pmm_ref_inc(pte & 0xFFFFF000u);
        }
    }

    // parent's ptes changed: flush its tlb
    uint32_t cr3;
    asm volatile("mov %%cr3, %0; mov %0, %%cr3" : "=r"(cr3) :: "memory");
    return out;
}

int paging_handle_cow(uint32_t vaddr) {
    uint32_t* pd = paging_current_pd_virt();
    uint32_t pde = pd[pde_index(vaddr)];
    if (!(pde & P_PRESENT)) return 0;

    uint32_t* pt = (uint32_t*)(pde & 0xFFFFF000u);
    uint32_t pti = pte_index(vaddr);
    uint32_t pte = pt[pti];
    if (!(pte & P_PRESENT) || (pte & P_RW) || !(pte & P_COW)) return 0;

    uint32_t frame = pte & 0xFFFFF000u;
    if (pmm_ref(frame) == 1) {
        // last owner: no copy needed
        pt[pti] = (pte | P_RW) & ~P_COW;
    } else {
        uintptr_t nf = pmm_alloc_frame();
        if (!nf) return 0;
        memcpy32((void*)nf, (void*)frame, PAGE_SIZE);
        pmm_free_frame(frame);                 // drop our share of the old frame
        pt[pti] = (nf & 0xFFFFF000u) | ((pte & 0xFFFu) | P_RW) & ~P_COW;
    }
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 1;
}

// slot layout: [guard page (unmapped)][KSTACK_BYTES of stack]
static uint32_t kstack_guard(uint32_t slot) {
    return KSTACK_BASE + slot * (KSTACK_BYTES + PAGE_SIZE);
}

uint32_t kstack_alloc(uint32_t slot) {
    if (slot >= KSTACK_SLOTS) return 0;
    uint32_t lo = kstack_guard(slot) + PAGE_SIZE;
    for (uint32_t off = 0; off < KSTACK_BYTES; off += PAGE_SIZE) {
        uintptr_t f = pmm_alloc_frame();
        if (!f || paging_map(lo + off, f, P_PRESENT | P_RW) < 0) {
            if (f) pmm_free_frame(f);
            kstack_free(slot);
            return 0;
        }
    }
    return lo;
}

void kstack_free(uint32_t slot) {
    uint32_t lo = kstack_guard(slot) + PAGE_SIZE;
    for (uint32_t off = 0; off < KSTACK_BYTES; off += PAGE_SIZE) {
        uint32_t phys = paging_translate(lo + off);
        if (!phys) continue;
        paging_unmap(lo + off);
        pmm_free_frame(phys & 0xFFFFF000u);
    }
}
