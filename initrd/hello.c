#include <kernel/syscall.h>

static int strlen_(const char* s) { int n = 0; while (s[n]) n++; return n; }

void _start(void) {
    const char* msg = "hello from userland!\n";
    __syscall3(SYS_write, 1, (int)msg, strlen_(msg));
    __syscall3(SYS_exit, 0, 0, 0);
    for (;;) {}
}
