#pragma once
#include <stdint.h>

// save eflags and disable interrupts; pass the result to irq_restore()
static inline uint32_t irq_save(void) {
    uint32_t f;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(f) :: "memory");
    return f;
}
static inline void irq_restore(uint32_t f) {
    __asm__ volatile("pushl %0; popfl" :: "r"(f) : "memory", "cc");
}
