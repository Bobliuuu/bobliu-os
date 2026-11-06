#include <sys/wait.h>
#include <errno.h>
#include <kernel/syscall.h>

int waitpid(int pid, int* status, int options) {
    int r = __syscall3(SYS_waitpid, pid, (int)status, options);
    if (r == -4) { errno = EINTR; return -1; }      /* a signal arrived while waiting */
    if (r < 0) { errno = ECHILD; return -1; }       /* no such child */
    return r;
}

int wait(int* status) {
    return waitpid(-1, status, 0);
}
