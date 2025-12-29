#include <kernel/panic.h>
#include <stdio.h>

__attribute__((noreturn))
void panic(const char* msg) {
    printf("PANIC: %s\n", msg ? msg : "(null)");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}
