#ifndef _SIGNAL_H
#define _SIGNAL_H 1

#define SIGHUP   1
#define SIGINT   2
#define SIGKILL  9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGTERM  15
#define SIGWINCH 28   /* defined for portability, never delivered (fixed-size console) */

typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#ifdef __cplusplus
extern "C" {
#endif

sighandler_t signal(int sig, sighandler_t handler);
int kill(int pid, int sig);

#ifdef __cplusplus
}
#endif

#endif
