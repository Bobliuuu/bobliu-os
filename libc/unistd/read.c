#include <unistd.h>
#include <kernel/syscall.h>

int read(int fd, void* buf, size_t count) {
    return __syscall3(SYS_read, fd, (int)buf, (int)count);
}
