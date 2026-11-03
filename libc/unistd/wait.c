#include <sys/wait.h>
#include <kernel/syscall.h>

int waitpid(int pid, int* status, int options) {
    return __syscall3(SYS_waitpid, pid, (int)status, options);
}

int wait(int* status) {
    return waitpid(-1, status, 0);
}
