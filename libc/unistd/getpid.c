#include <unistd.h>
#include <kernel/syscall.h>

int getpid(void) {
    return __syscall3(SYS_getpid, 0, 0, 0);
}
