// timer.c
#include <stdint.h>
#include <arch/i386/portio.h>
#include <arch/i386/isr.h>

extern int printf(const char*, ...);
extern void irq_install_handler(int irq, void (*fn)(regs_t*));

static volatile uint64_t ticks = 0;

uint32_t timer_ticks(void) { 
    return ticks; 
}

static void timer_cb(regs_t* r) {
    (void)r;
    ticks++;
    // uncomment if you want a tick
    //if ((ticks % 100) == 0) printf("[tick %llu]\n", ticks);
    if ((ticks % 100) == 0) putchar('.');
}

void timer_init(uint32_t hz) {
    // PIT base frequency
    uint32_t divisor = 1193180 / hz;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install_handler(0, timer_cb); // IRQ0
}

static void timer_irq(regs_t* r) {
    (void)r;
    ticks++;
    if ((ticks % 100) == 0) putchar('.');   // prints dot periodically
}