#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int write(int fd, const void* buf, size_t count);
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
int open(const char* path, int flags, ...);   /* mode argument is accepted and ignored */
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
int pipe2(int fds[2], int flags);
int dup(int fd);
int dup2(int oldfd, int newfd);
int getpid(void);
int fork(void);
extern char** environ;
int execv(const char* path, char* const argv[]);   /* argv is passed to main() */
int execve(const char* path, char* const argv[], char* const envp[]);
__attribute__((__noreturn__)) void _exit(int status);
int ftruncate(int fd, off_t length);      /* length 0 or the current size only */
int fsync(int fd);
int fdatasync(int fd);
int access(const char* path, int mode);
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
pid_t getppid(void);
pid_t getpgrp(void);
pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);
pid_t setsid(void);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int chown(const char* path, uid_t u, gid_t g);
int gethostname(char* name, size_t len);
int rmdir(const char* path);
long sysconf(int name);
#define _SC_ARG_MAX 0
#define _SC_CLK_TCK 2
#define _SC_OPEN_MAX 4
#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE 30
#define _SC_NPROCESSORS_CONF 83
#define _SC_NPROCESSORS_ONLN 84
#define _SC_GETPW_R_SIZE_MAX 70
#define _SC_HOST_NAME_MAX 180
int dup3(int oldfd, int newfd, int flags);
int execvp(const char* file, char* const argv[]);
int execl(const char* path, const char* arg, ...);
int execlp(const char* file, const char* arg, ...);
int link(const char* a, const char* b);
int symlink(const char* a, const char* b);
ssize_t readlink(const char* path, char* buf, size_t n);
int utimes_stub(void);
unsigned alarm(unsigned seconds);
int pause(void);
ssize_t pread(int fd, void* buf, size_t n, off_t off);
ssize_t pwrite(int fd, const void* buf, size_t n, off_t off);
char* ttyname(int fd);
int ttyname_r(int fd, char* buf, size_t n);
int fchown(int fd, uid_t u, gid_t g);
int lockf(int fd, int cmd, off_t len);
int getpagesize(void);
int chroot(const char* p);

#ifdef __cplusplus
}
#endif

#endif
