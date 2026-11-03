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
int isatty(int fd);
int tcsetraw(int on);   /* 1: raw console (read gives bytes, no echo, ^C = byte 3), 0: cooked, 2: raw scancodes (see fb.h) */
int brk(void* addr);
void* sbrk(int incr);
unsigned sleep(unsigned seconds);   /* returns seconds left if a signal cut it short (all of it, for now) */
int usleep(unsigned usec);
int pipe(int fds[2]);
int dup(int fd);
int dup2(int oldfd, int newfd);
int getpid(void);
int fork(void);
extern char** environ;
int execv(const char* path, char* const argv[]);   /* argv is passed to main() */
int execve(const char* path, char* const argv[], char* const envp[]);
__attribute__((__noreturn__)) void _exit(int status);

#ifdef __cplusplus
}
#endif

#endif
