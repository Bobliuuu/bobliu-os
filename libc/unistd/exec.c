#include <unistd.h>
#include <kernel/syscall.h>

// only the path is used: no argv/envp passing yet
int execve(const char* path, char* const argv[], char* const envp[]) {
    (void)argv; (void)envp;
    return __syscall3(SYS_execve, (int)path, 0, 0);
}

int execv(const char* path, char* const argv[]) {
    return execve(path, argv, 0);
}
