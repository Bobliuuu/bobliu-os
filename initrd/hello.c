#include <stdint.h>

static inline void sys_exit(int code) {
    asm volatile(
        "movl $2, %%eax\n"   // SYS_exit = 2
        "movl %0, %%ebx\n"
        "int $0x80\n"
        :
        : "r"(code)
        : "eax", "ebx"
    );
    __builtin_unreachable();
}

void _start(void) {
    const char* s = "hello from userland!\n";
    while (*s) {
        // your sys_putchar
        uint32_t v = (uint32_t)*s++;
        asm volatile(
            "movl $1, %%eax\n"
            "movl %0, %%ebx\n"
            "int $0x80\n"
            :
            : "r"(v)
            : "eax","ebx"
        );
    }
    sys_exit(0);
}
