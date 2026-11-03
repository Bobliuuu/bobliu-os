#include <unistd.h>
#include <kernel/syscall.h>

int __io_ret(int r);

int write(int fd, const void* buf, size_t count) {
    return __io_ret(__syscall3(SYS_write, fd, (int)buf, (int)count));
}
