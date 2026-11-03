#include <unistd.h>
#include <errno.h>
#include <kernel/syscall.h>

/* the kernel returns -11 (EAGAIN) / -4 (EINTR) for those two, plain -1 for everything else */
int __io_ret(int r) {
    if (r >= -1) return r;
    errno = r == -11 ? EAGAIN : r == -4 ? EINTR : EIO;
    return -1;
}

int read(int fd, void* buf, size_t count) {
    return __io_ret(__syscall3(SYS_read, fd, (int)buf, (int)count));
}
