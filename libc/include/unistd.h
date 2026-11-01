#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int write(int fd, const void* buf, size_t count);
int open(const char* path, int flags);
int close(int fd);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int unlink(const char* path);   /* files and empty directories */
int lseek(int fd, int offset, int whence);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int read(int fd, void* buf, size_t count);   /* fd 0 only: line buffered console */
int brk(void* addr);
void* sbrk(int incr);
int dup(int fd);
int dup2(int oldfd, int newfd);
int getpid(void);
int fork(void);
int execv(const char* path, char* const argv[]);   /* argv is passed to main() */
int execve(const char* path, char* const argv[], char* const envp[]);
__attribute__((__noreturn__)) void _exit(int status);

#ifdef __cplusplus
}
#endif

#endif
