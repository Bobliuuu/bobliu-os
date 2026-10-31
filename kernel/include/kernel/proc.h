#pragma once
#include <stdint.h>
#include <arch/i386/isr.h>
#include <arch/i386/paging.h>

#define NPROC       16
#define KSTACK_SIZE 16384

typedef enum {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_SLEEPING,   // blocked in waitpid
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
    page_directory_t pd;
    uint8_t*       kstack;      // 0 for the boot/kernel context (pid 0)
    struct context* ctx;        // saved kernel context while not running
    regs_t*        tf;          // trap frame of the current syscall/irq
    char           name[16];
} proc_t;

void    proc_init(void);
proc_t* proc_current(void);
int     proc_spawn(const char* path);               // new process running path; returns pid or -1
int     proc_fork(regs_t* tf);                      // child pid to parent, child resumes with 0
int     proc_exec(const char* path, regs_t* tf);    // replace current image; 0 or -1
int     proc_wait(int pid, int* status);            // pid, -1 if nothing to wait for
__attribute__((noreturn)) void proc_exit(int code);
void    proc_yield(void);
void    proc_reap_orphans(void);                    // kernel context: reap zombies nobody waits for
int     proc_count(void);
void    proc_dump(void);                            // for the shell's ps

// user_exec.c: build a fresh address space for path
int user_load(const char* path, page_directory_t* out_pd, uint32_t* entry);
