#include <unistd.h>
#include <kernel/syscall.h>

int brk(void* addr) {
    int r = __syscall3(SYS_brk, (int)addr, 0, 0);
    return r == -1 ? -1 : 0;
}

void* sbrk(int incr) {
    int cur = __syscall3(SYS_brk, 0, 0, 0);
    if (incr == 0) return (void*)cur;
    int r = __syscall3(SYS_brk, cur + incr, 0, 0);
    return r == -1 ? (void*)-1 : (void*)cur;
}
