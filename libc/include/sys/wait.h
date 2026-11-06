#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H 1

#ifdef __cplusplus
extern "C" {
#endif

int waitpid(int pid, int* status, int options);
int wait(int* status);

#define WNOHANG 1
#define WEXITSTATUS(s) (((s) >> 8) & 0xFF)
#define WIFEXITED(s)   (1)
#define WIFSIGNALED(s) (0)   /* the kernel reports a killed process as an exit status */
#define WTERMSIG(s)    (0)
#define WIFSTOPPED(s)  (0)
#define WSTOPSIG(s)    (0)
#define WUNTRACED 2

#ifdef __cplusplus
}
#endif

#endif
