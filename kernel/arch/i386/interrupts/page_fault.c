#include <stdint.h>
#include <arch/i386/isr.h>   // for regs_t
#include <stdio.h>
#include <kernel/proc.h>
#include <arch/i386/paging.h>

extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);


#define PF_TMP_PT_VA 0x00FF0000u  // must be in your kernel-mapped region (0..16MB)

static inline uint32_t read_cr2(void) {
    uint32_t v;
    asm volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

static void print_pf_bits(uint32_t err) {
    // bit0 P: 0=not-present, 1=protection violation
    // bit1 W/R: 0=read, 1=write
    // bit2 U/S: 0=kernel, 1=user
    vga_print((err & 1) ? "P " : "NP ");
    vga_print((err & 2) ? "W " : "R ");
    vga_print((err & 4) ? "U " : "K ");
}

static void pf_walk(uint32_t cr2) {
    uint32_t* pd = paging_current_pd_virt();

    uint32_t va  = cr2;
    uint32_t pdi = (va >> 22) & 0x3FF;
    uint32_t pti = (va >> 12) & 0x3FF;

    uint32_t pde = pd[pdi];

    vga_print("PF walk: va=");
    vga_print_hex(va);
    vga_print(" pdi=");
    vga_print_hex(pdi);
    vga_print(" pti=");
    vga_print_hex(pti);
    vga_print(" pde=");
    vga_print_hex(pde);
    vga_print("\n");

    if (!(pde & P_PRESENT)) {
        vga_print("PF walk: no PDE present\n");
        return;
    }

    uint32_t pt_phys = pde & 0xFFFFF000u;

    paging_map(PF_TMP_PT_VA, pt_phys, P_PRESENT | P_RW);
    uint32_t* pt = (uint32_t*)PF_TMP_PT_VA;

    uint32_t pte = pt[pti];

    vga_print("PF walk: pt_phys=");
    vga_print_hex(pt_phys);
    vga_print(" pte=");
    vga_print_hex(pte);
    vga_print("\n");

    paging_unmap(PF_TMP_PT_VA);
}

void page_fault_handler(regs_t* r) {
    uint32_t cr2 = read_cr2();

    // write to a present read-only page that is really a shared cow page (user or kernel mode)
    if ((r->err_code & 3) == 3 && cr2 >= USER_LO && cr2 < USER_HI && paging_handle_cow(cr2))
        return;

    if (r->cs & 3) {
        printf("\n[pid %d killed: segfault at %x, eip=%x]\n", proc_current()->pid, cr2, r->eip);
        proc_exit(139);
    }

    if (cr2 >= KSTACK_BASE && cr2 < KSTACK_BASE + KSTACK_SLOTS * (KSTACK_BYTES + PAGE_SIZE) &&
        ((cr2 - KSTACK_BASE) % (KSTACK_BYTES + PAGE_SIZE)) < PAGE_SIZE) {
        vga_print("\nKERNEL STACK OVERFLOW (guard page hit)\n");
    }

    vga_print("\nPAGE FAULT: cr2=");
    vga_print_hex(cr2);
    vga_print(" eip=");
    vga_print_hex(r->eip);
    vga_print(" err=");
    vga_print_hex(r->err_code);
    vga_print(" ");
    print_pf_bits(r->err_code);
    vga_print("\n");

    pf_walk(cr2);
    
    for (;;) { asm volatile("cli; hlt"); }
}
