#include <unistd.h>
#include <kernel/syscall.h>

extern char** environ;

int execve(const char* path, char* const argv[], char* const envp[]) {
    return __syscall3(SYS_execve, (int)path, (int)argv, (int)envp);
}

// like POSIX: the program gets the caller's environment
int execv(const char* path, char* const argv[]) {
    return execve(path, argv, environ);
}
