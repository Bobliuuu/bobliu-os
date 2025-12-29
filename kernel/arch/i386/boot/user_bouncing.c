#include <stdint.h>
#include <arch/i386/user_bounce.h>

volatile uint32_t g_user_exited = 0;
volatile uint32_t g_exec_kesp = 0;
volatile uint32_t g_exec_resume_eip = 0;

// implemented by you already (your enter_user iret helper)
extern void enter_user(uint32_t entry, uint32_t user_stack_top) __attribute__((noreturn));

static inline uint32_t read_esp(void) {
    uint32_t v;
    asm volatile("mov %%esp, %0" : "=r"(v));
    return v;
}

int exec_enter_usermode(uint32_t entry, uint32_t user_stack_top) {
    // Save kernel stack pointer
    //asm volatile("mov %%esp, %0" : "=r"(g_exec_kesp));
    g_exec_kesp = read_esp();
    
    // Save resume address (label address)
    void* resume = &&resume_from_user;
    g_exec_resume_eip = (uint32_t)resume;

    asm volatile("" ::: "memory");
    
    printf("exec 1: entry=%x user_stack_top=%x\n", entry, user_stack_top);
    printf("exec: entry_phys=%x stack_page_phys=%x\n",
       paging_translate(entry),
       paging_translate(user_stack_top - 0x1000));
    // enter ring3 (never returns normally)
    enter_user(entry, user_stack_top);

    // should never hit
    return -1;

resume_from_user:
    // We land here after SYS_exit triggers trampoline
    return 0;
}
