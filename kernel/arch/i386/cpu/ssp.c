#include <stdint.h>
#include <stdio.h>

__attribute__((used))
uintptr_t __stack_chk_guard = 0;

// If you have a better entropy source later, replace this.
// For now: a fixed canary is totally fine for catching bugs in dev.
static uintptr_t default_guard(void) {
#if UINTPTR_MAX == 0xFFFFFFFFu
    return (uintptr_t)0xBAADF00Du;
#else
    return (uintptr_t)0xBAADF00DBAADF00Dull;
#endif
}

// Called very early (from kernel_main) to initialize the canary
void ssp_init(void) {
    if (__stack_chk_guard == 0) {
        __stack_chk_guard = default_guard();
    }
}

// GCC will call this when it sees stack corruption
__attribute__((noreturn))
void __stack_chk_fail(void) {
    volatile uint16_t* vga = (uint16_t*)0xB8000;
    vga[0] = (0x4F << 8) | '!';   // red '!' at top-left
    // Will not run, gcc cannot deal with stack corruption yet
    printf("\n\n*** STACK SMASH DETECTED ***\n");
    printf("Halting.\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

// Some toolchains call the local variant
__attribute__((noreturn))
void __stack_chk_fail_local(void) {
    __stack_chk_fail();
}
