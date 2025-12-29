// syscall_exit.c
#include <stdio.h>
#include <kernel/shell.h>
#include <arch/i386/keyboard.h>

extern void shell_prompt_public(void); // expose this (see below)

__attribute__((noreturn))
void syscall_exit_bounce(void) {
    printf("\n[proc exited]\n");

    // IMPORTANT: re-enable shell input path in keyboard driver
    keyboard_enable_shell(true);

    // Re-print prompt (don’t re-run full shell_init banner if you don’t want)
    shell_prompt_public();

    // Go back to idle. IRQs are enabled (sti already done in asm).
    for (;;) asm volatile("hlt");
}
