#include <unistd.h>
#include <kernel/syscall.h>

// envp is not passed yet
int execve(const char* path, char* const argv[], char* const envp[]) {
    (void)envp;
    return __syscall3(SYS_execve, (int)path, (int)argv, 0);
}

int execv(const char* path, char* const argv[]) {
    return execve(path, argv, 0);
}
