#include <kernel/syscall.h>

static int len(const char* s) { int n = 0; while (s[n]) n++; return n; }

int main(void) {
    const char* msg = "hello from userland!\n";
    __syscall3(SYS_write, 1, (int)msg, len(msg));
    return 0;
}
