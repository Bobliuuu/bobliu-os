#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H 1

#ifdef __cplusplus
extern "C" {
#endif

int waitpid(int pid, int* status, int options);
int wait(int* status);

#define WEXITSTATUS(s) (((s) >> 8) & 0xFF)
#define WIFEXITED(s)   (1)

#ifdef __cplusplus
}
#endif

#endif
