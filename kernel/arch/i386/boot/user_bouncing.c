#include <stdint.h>
#include <arch/i386/user_bouncing.h>
#include <arch/i386/portio.h>
#include <stdio.h>

volatile uint32_t g_user_exited = 0;
volatile uint32_t g_exec_kesp = 0;
volatile uint32_t g_exec_resume_eip = 0;
volatile uint32_t g_exec_kcr3 = 0;

volatile uint32_t dbg_iret_eip    = 0;
volatile uint32_t dbg_iret_cs     = 0;
volatile uint32_t dbg_iret_eflags = 0;
volatile uint32_t dbg_iret_esp    = 0;
volatile uint32_t dbg_iret_ss     = 0;

// asm file instead
extern void enter_user(uint32_t entry, uint32_t user_stack_top) __attribute__((noreturn));

static inline uint32_t read_esp(void) {
    uint32_t v;
    asm volatile("mov %%esp, %0" : "=r"(v));
    return v;
}

void exec_mark_kernel_stack(void) {
    g_exec_kesp = read_esp();
}

/*
__attribute__((noinline))
int exec_enter_usermode(uint32_t entry, uint32_t user_stack_top) {
    (void)entry; (void)user_stack_top;
    // Should never be reached
    for (;;) asm volatile("hlt");
}
*/

static inline uint32_t read_eflags(void) {
    uint32_t e;
    __asm__ volatile("pushfl; popl %0" : "=r"(e));
    return e;
}

void pic_unmask_irq1(void) {
    uint8_t mask = inb(0x21);      // master PIC mask
    printf("picmask(before)=%x\n", mask);
    mask &= ~(1 << 1);             // clear bit 1 = IRQ1
    outb(0x21, mask);
    printf("picmas(after)=%x\n", inb(0x21));
}

void exec_resumed_debug(void) {
    uint8_t m = inb(0x21);
    printf("picmask(before)=%x\n", m);
    outb(0x21, m & ~(1<<1));
    printf("picmask(after)=%x\n", inb(0x21));
    uint32_t e = read_eflags();
    printf("\n[resumed kernel after user] eflags=%x IF=%u\n",
           e, (e & 0x200) ? 1 : 0);
}