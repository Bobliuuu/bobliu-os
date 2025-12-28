#pragma once
#include <stdint.h>

typedef struct regs {
    uint32_t ds;

    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    uint32_t int_no, err_code;

    uint32_t eip, cs, eflags, useresp, ss;
} regs_t;

// Generic ISR/IRQ handler signature
typedef void (*isr_t)(regs_t*);

void isr_handler(regs_t* r);
void irq_handler(regs_t* r);

// Install/uninstall an IRQ handler by IRQ line number (0..15):
// 0=timer, 1=keyboard, ...
void irq_install_handler(int irq, isr_t handler);
void irq_uninstall_handler(int irq);
