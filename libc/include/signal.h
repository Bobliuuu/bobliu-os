#ifndef _SIGNAL_H
#define _SIGNAL_H 1
#include <sys/types.h>

#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGTRAP  5
#define SIGABRT  6
#define SIGBUS   7
#define SIGFPE   8
#define SIGKILL  9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGCHLD  17
#define SIGCONT  18
#define SIGSTOP  19
#define SIGTSTP  20
#define SIGTTIN  21
#define SIGTTOU  22
#define SIGWINCH 28   /* defined for portability, never delivered (fixed-size console) */
#define NSIG 32

typedef void (*sighandler_t)(int);
typedef sighandler_t sig_t;
typedef int sig_atomic_t;
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

typedef unsigned int sigset_t;
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#define SA_NOCLDSTOP 1
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000
#define SA_RESTART 0x10000000
#define SA_SIGINFO 4

typedef struct { int si_signo, si_errno, si_code; pid_t si_pid; uid_t si_uid; void* si_addr; int si_status; } siginfo_t;
struct sigaction {
    union { void (*sa_handler)(int); void (*sa_sigaction)(int, siginfo_t*, void*); };
    sigset_t sa_mask;
    int sa_flags;
};

#ifdef __cplusplus
extern "C" {
#endif

sighandler_t signal(int sig, sighandler_t handler);
int kill(int pid, int sig);
int raise(int sig);
int sigaction(int sig, const struct sigaction* act, struct sigaction* old);   /* mask is recorded, not enforced */
int sigprocmask(int how, const sigset_t* set, sigset_t* old);
int pthread_sigmask(int how, const sigset_t* set, sigset_t* old);
int sigemptyset(sigset_t* s);
int sigfillset(sigset_t* s);
int sigaddset(sigset_t* s, int sig);
int sigdelset(sigset_t* s, int sig);
int sigismember(const sigset_t* s, int sig);

#ifdef __cplusplus
}
#endif

#endif
