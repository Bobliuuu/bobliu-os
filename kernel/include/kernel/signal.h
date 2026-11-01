#pragma once
#include <stdint.h>
#include <arch/i386/isr.h>

#define NSIG 32
#define SIGHUP   1
#define SIGINT   2
#define SIGKILL  9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGTERM  15
#define SIG_DFL  0u
#define SIG_IGN  1u

struct proc;

int  sig_send(struct proc* p, int sig);      // 0, or -1 if the signal number is bad / p is not alive
void sig_broadcast(int sig);                 // every live user process (ctrl-c)
int  sig_interrupting(void);                 // current process has a deliverable signal: blocking calls give up
void sig_deliver(regs_t* tf);                // on the way back to ring 3: run default action or enter a handler
int  sig_set(int sig, uint32_t handler, uint32_t restorer);   // old handler, or -1
void sig_return(regs_t* tf);                 // sigreturn syscall
void sig_exec_reset(struct proc* p);         // caught handlers go back to default on exec
