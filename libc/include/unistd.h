#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int write(int fd, const void* buf, size_t count);
int read(int fd, void* buf, size_t count);   /* fd 0 only: line buffered console */
int getpid(void);
int fork(void);
int execv(const char* path, char* const argv[]);   /* argv is passed to main() */
int execve(const char* path, char* const argv[], char* const envp[]);
__attribute__((__noreturn__)) void _exit(int status);

#ifdef __cplusplus
}
#endif

#endif
