#include <stdint.h>
#include <arch/i386/isr.h>   // for regs_t
#include <stdio.h>
#include <arch/i386/paging.h>

extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);

extern volatile uint32_t dbg_iret_eip, dbg_iret_cs, dbg_iret_eflags, dbg_iret_esp, dbg_iret_ss;

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

    vga_print("\nPAGE FAULT: cr2=");
    vga_print_hex(cr2);
    vga_print(" eip=");
    vga_print_hex(r->eip);
    vga_print(" err=");
    vga_print_hex(r->err_code);
    vga_print(" ");
    print_pf_bits(r->err_code);
    vga_print("\n");

    vga_print("IRET frame (last): eip=");
    vga_print_hex(dbg_iret_eip);
    vga_print("cs=");
    vga_print_hex(dbg_iret_cs);
    vga_print("eflags=");
    vga_print_hex(dbg_iret_eflags);
    vga_print("esp=");
    vga_print_hex(dbg_iret_esp);
    vga_print("ss=");
    vga_print_hex(dbg_iret_ss);

    pf_walk(cr2);
    
    for (;;) { asm volatile("cli; hlt"); }
}
