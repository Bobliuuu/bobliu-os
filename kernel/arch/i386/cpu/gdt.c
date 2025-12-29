// gdt.c
#include <stdint.h>
#include <arch/i386/gdt.h>

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
};

struct __attribute__((packed)) gdt_ptr {
    uint16_t limit;
    uint32_t base;
};

static struct gdt_entry gdt[6];
static struct gdt_ptr gp;

extern void gdt_flush(uint32_t gp_addr);

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low  = base & 0xFFFF;
    gdt[i].base_mid  = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;

    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].gran      = (limit >> 16) & 0x0F;
    gdt[i].gran     |= gran & 0xF0;
    gdt[i].access    = access;
}

static void dump_gdt_entry5(void) {
    // print the raw 8 bytes for gdt[5]
    extern uint8_t gdt_raw[]; // not available unless you expose it
}

void gdt_set_tss(uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // put TSS at index 5
    gdt_set(5, base, limit, access, gran);
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt[0];

    gdt_set(0, 0, 0, 0, 0);

    // kernel code/data
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // 0x08
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // 0x10

    // user code/data
    gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // 0x18 (use as 0x1B in ring3)
    gdt_set(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // 0x20 (use as 0x23 in ring3)

    // index 5 reserved for TSS; filled by tss_init()

    gdt_flush((uint32_t)&gp);
}