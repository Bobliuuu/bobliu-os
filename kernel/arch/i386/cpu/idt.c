// idt.c
#include <stdint.h>
#include <arch/i386/idt.h>

struct __attribute__((packed)) idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
};

struct __attribute__((packed)) idt_ptr {
    uint16_t limit;
    uint32_t base;
};

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void idt_load(uint32_t idtp_addr);

// ISR stubs (exceptions 0..31)
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

// IRQ stubs (we’ll map to 32..47)
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags; // 0x8E = present, ring0, 32-bit interrupt gate
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt[0];

    // Clear
    for (int i = 0; i < 256; i++) {
        idt[i] = (struct idt_entry){0};
    }

    // Exceptions
    void* isrs[32] = {
        isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,
        isr8,isr9,isr10,isr11,isr12,isr13,isr14,isr15,
        isr16,isr17,isr18,isr19,isr20,isr21,isr22,isr23,
        isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31
    };
    for (int i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, (uint32_t)isrs[i], 0x08, 0x8E);
    }

    // IRQs 0..15 => vectors 32..47
    void* irqs[16] = {
        irq0,irq1,irq2,irq3,irq4,irq5,irq6,irq7,
        irq8,irq9,irq10,irq11,irq12,irq13,irq14,irq15
    };
    for (int i = 0; i < 16; i++) {
        idt_set_gate((uint8_t)(32+i), (uint32_t)irqs[i], 0x08, 0x8E);
    }

    idt_load((uint32_t)&idtp);
}
