#include <stdint.h>
#include <arch/i386/isr.h>   // for regs_t

extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);

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

    for (;;) { asm volatile("cli; hlt"); }
}
