// isr.c
#include <arch/i386/isr.h>
#include <arch/i386/portio.h>
#include <arch/i386/user_bounce.h>
#include <stdio.h>

extern volatile uint32_t g_exec_kesp;
extern volatile uint32_t g_exec_resume_eip;
extern volatile uint32_t g_user_exited;

// use your kernel printf
extern int printf(const char*, ...);
// Page fault handler
void page_fault_handler(regs_t* r);
// Syscall handler for userspace
extern void syscall_handle(regs_t* r);

static const char* exc_names[32] = {
    "Divide-by-zero","Debug","NMI","Breakpoint","Overflow","Bound Range",
    "Invalid Opcode","Device Not Available","Double Fault","Coprocessor Segment",
    "Invalid TSS","Segment Not Present","Stack-Segment Fault","General Protection",
    "Page Fault","Reserved","x87 FP Exception","Alignment Check","Machine Check",
    "SIMD FP Exception","Virtualization","Control Protection","Reserved","Reserved",
    "Reserved","Reserved","Reserved","Reserved","Hypervisor Injection","VMM Communication",
    "Security","Reserved"
};

/*
static __attribute__((noreturn)) 
void exec_bounce_to_kernel(void) {
    asm volatile(
        "cli\n"
        "mov %0, %%esp\n"
        "jmp *%1\n"
        :
        : "r"(g_exec_kesp), "r"(g_exec_resume_eip)
        : "memory"
    );
    __builtin_unreachable();
}
*/

void isr_handler(regs_t* r) {
    if (r->int_no == 14) {
        page_fault_handler(r);
    }
    if (r->int_no == 128) {
        isr128_handler(r);
        return;
    }
    if (r->int_no == 0x80) {
        syscall_handle(r);
        return;
    }

    if (r->int_no < 32) {
        printf("\n\n[EXCEPTION %u] %s  err=%u\n", r->int_no, exc_names[r->int_no], r->err_code);
        printf("EIP=%x CS=%x EFLAGS=%x\n", r->eip, r->cs, r->eflags);
        printf("EAX=%x EBX=%x ECX=%x EDX=%x\n", r->eax, r->ebx, r->ecx, r->edx);
        printf("ESI=%x EDI=%x EBP=%x ESP=%x\n", r->esi, r->edi, r->ebp, r->esp);
        // Halt forever so you can read it
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }
}

static inline void pic_send_eoi(unsigned int int_no) {
    // IRQs are mapped to 0x20..0x2F
    if (int_no >= 0x28) outb(0xA0, 0x20); // slave PIC
    outb(0x20, 0x20);                     // master PIC
}

// Simple IRQ dispatch table
typedef void (*irq_fn)(regs_t*);
static irq_fn irq_routines[16] = {0};

void irq_install_handler(int irq, irq_fn fn) { irq_routines[irq] = fn; }
void irq_uninstall_handler(int irq) { irq_routines[irq] = 0; }

void irq_handler(regs_t* r) {
    int irq = (int)r->int_no - 32;

    if (irq >= 0 && irq < 16 && irq_routines[irq]) {
        irq_routines[irq](r);
    }

    // Send EOI
    //if (r->int_no >= 40) outb(0xA0, 0x20);
    //outb(0x20, 0x20);

    pic_send_eoi(r->int_no);
}

void isr128_handler(regs_t* r) {
    // convention: eax = syscall number, ebx/ecx/edx = args
    // for testing: syscall 1 => print a char in BL
    if (r->eax == 1) {
        putchar((char)(r->ebx & 0xFF));
        return;
    }

    // syscall 2 => print string at user pointer EBX (unsafe!!!) -> need copy + validation
    if (r->eax == 2) { // exit(code)
        printf("\n[proc exited]\n");
        printf("exit: useresp=%x saved_kesp=%x resume=%x\n",
        (uint32_t)r->useresp, (uint32_t)g_exec_kesp, (uint32_t)g_exec_resume_eip);
        //exec_bounce_to_kernel();
        g_user_exited = 1;
        return;
    }

    return;
}
