#include <unistd.h>
#include <kernel/syscall.h>

int fork(void) {
    return __syscall3(SYS_fork, 0, 0, 0);
}
