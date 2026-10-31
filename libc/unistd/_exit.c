#include <unistd.h>
#include <kernel/syscall.h>

void _exit(int status) {
    __syscall3(SYS_exit, status, 0, 0);
    for (;;) { }
}
