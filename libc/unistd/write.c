#include <unistd.h>
#include <kernel/syscall.h>

int write(int fd, const void* buf, size_t count) {
    return __syscall3(SYS_write, fd, (int)buf, (int)count);
}
