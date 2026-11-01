#pragma once
#include <stdint.h>
#include <arch/i386/isr.h>
#include <arch/i386/paging.h>

#define NPROC       16
#define PROC_MAX_ARGS 16
#define PROC_ARG_LEN  128
#define PROC_MAXFD  16   /* fd table entry: vfs fd >= 0, FD_CONSOLE, or -1 (free) */
#define FD_CONSOLE  (-2)
#define KSTACK_SIZE KSTACK_BYTES   /* see arch/i386/paging.h */

typedef enum {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_SLEEPING,   // blocked: waitpid (chan 0) or console input (chan 1)
    PROC_ZOMBIE      // exited, waiting for the parent to reap
} proc_state_t;

// callee-saved regs pushed by swtch(); eip is the return address
struct context {
    uint32_t edi, esi, ebx, ebp, eip;
};

typedef struct proc {
    int            pid;
    proc_state_t   state;
    struct proc*   parent;
    int            exit_code;
    int            chan;        // what a SLEEPING proc waits for
    page_directory_t pd;
    uint8_t*       kstack;      // 0 for the boot/kernel context (pid 0)
    struct context* ctx;        // saved kernel context while not running
    regs_t*        tf;          // trap frame of the current syscall/irq
    uint32_t       brk;         // end of the user heap (starts at USER_HEAP_BASE)
    int            fds[PROC_MAXFD];   // user fd -> vfs fd / FD_CONSOLE / -1; inherited by fork and exec
    char           cwd[PROC_ARG_LEN];   // absolute; inherited by fork/exec
    char           name[16];
} proc_t;

void    proc_init(void);
int     proc_fd_alloc(int entry);       // lowest free user fd, or -1
int     proc_fd_get(int ufd);           // vfs fd, FD_CONSOLE or -1
int     proc_fd_close(int ufd);
int     proc_fd_dup(int ofd, int nfd);  // dup2 (nfd < 0: dup)
proc_t* proc_current(void);
#define WAIT_CHILD   0
#define WAIT_CONSOLE 1

int     proc_spawn(const char* path, char** argv, int argc);   // returns pid or -1
int     proc_fork(regs_t* tf);                      // child pid to parent, child resumes with 0
int     proc_exec(const char* path, char** argv, int argc, regs_t* tf);  // replace image; 0 or -1
int     proc_wait(int pid, int* status);            // pid, -1 if nothing to wait for
__attribute__((noreturn)) void proc_exit(int code);
void    proc_yield(void);
void    proc_sleep_on(int chan);                    // irqs must be off; returns once woken
void    proc_wakeup(int chan);
int     proc_kernel_busy(void);                     // kernel context is blocked waiting on a child
void    proc_reap_orphans(void);                    // kernel context: reap zombies nobody waits for
int     proc_brk(uint32_t addr);                    // new break, or -1; addr 0 just queries
int     proc_count(void);
void    proc_dump(void);                            // for the shell's ps

// user_exec.c: build a fresh address space for path
int user_load(const char* path, char** argv, int argc, page_directory_t* out_pd, uint32_t* entry, uint32_t* sp);
