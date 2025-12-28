// interrupts.c
#include <arch/i386/gdt.h>
#include <arch/i386/idt.h>
#include <arch/i386/pic.h>

void timer_init(unsigned hz);
void keyboard_init(void);

void interrupts_init(void) {
    gdt_init();
    idt_init();

    pic_remap(0x20, 0x28); // 32, 40
    // Unmask only timer(IRQ0) and keyboard(IRQ1) for now
    pic_clear_mask(0);
    pic_clear_mask(1);

    timer_init(100);       // 100 Hz
    keyboard_init();

    __asm__ volatile ("sti");
}
