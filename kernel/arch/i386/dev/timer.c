// timer.c
#include <stdint.h>
#include <arch/i386/portio.h>
#include <arch/i386/isr.h>
#include <arch/i386/timer.h>

extern int printf(const char*, ...);
extern void irq_install_handler(int irq, void (*fn)(regs_t*));

static volatile uint64_t ticks = 0;

uint32_t timer_ticks(void) {
    return (uint32_t)ticks;
}

uint64_t timer_ms(void) { return ticks * (1000 / TIMER_HZ); }

extern void proc_yield(void);
extern void proc_wakeup(int chan);
#define WAIT_TIMER 2   /* keep in sync with kernel/proc.h */

static void timer_cb(regs_t* r) {
    ticks++;
    proc_wakeup(WAIT_TIMER);        // nanosleep() callers re-check their deadline
    // preempt only user-mode code, every 5th tick (50ms)
    if ((r->cs & 3) && (ticks % 5) == 0) proc_yield();
    // uncomment if you want a tick
    //if ((ticks % 100) == 0) printf("[tick %llu]\n", ticks);
}

void timer_init(uint32_t hz) {
    // PIT base frequency
    uint32_t divisor = 1193180 / hz;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install_handler(0, timer_cb); // IRQ0
}
