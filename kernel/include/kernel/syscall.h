#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

/*
 * syscall ABI (int 0x80), shared by kernel and user programs.
 *   eax = number, ebx/ecx/edx = args 1-3, return value in eax (-1 on error)
 * numbers follow linux i386 so existing docs/toolchains line up.
 */
#define SYS_exit    1
#define SYS_fork    2
#define SYS_read    3
#define SYS_write   4
#define SYS_open    5
#define SYS_close   6
#define SYS_waitpid 7
#define SYS_execve  11
#define SYS_time    13   /* time(time_t*) -> unix seconds (utc) */
#define SYS_chdir   12
#define SYS_nanosleep 162 /* nanosleep(struct bob_timespec* req): 0, or -1 if a signal cut it short */
#define SYS_getcwd  183  /* getcwd(buf, size) -> length incl. NUL, or -1 */
#define SYS_getpid  20
#define SYS_unlink  10
#define SYS_lseek   19
#define SYS_clone   120  /* clone(struct bob_clone*): new thread sharing the address space; returns its tid */
#define SYS_sched_yield 158
#define SYS_gettid  224
#define SYS_futex   240  /* futex(addr, op, val): op 0 = sleep if *addr == val, 1 = wake sleepers on addr */
#define SYS_thread_exit 300   /* thread_exit(code): ends this thread (the main thread ending itself ends the process) */
#define SYS_thread_join 301   /* thread_join(tid, int* code) -> tid, or -1 */
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define SYS_kill    37
#define SYS_signal  48   /* signal(sig, handler, restorer) -> old handler; 0 = default, 1 = ignore */
#define SYS_sigreturn 119
#define SYS_mkdir   39
#define SYS_dup     41
#define SYS_pipe    42   /* pipe(int fds[2]): fds[0] read end, fds[1] write end */
#define SYS_brk     45   /* brk(addr): new break or -1; brk(0) queries */
#define SYS_dup2    63
#define SYS_readdir 89   /* readdir(path, index, namebuf[128]) -> 1 / 0 at end / -1 */
#define SYS_tcsetraw 200 /* tcsetraw(on): 1 = raw console (bytes, no echo), 0 = cooked; reset on exit */
#define SYS_fb_init 201  /* fb_init(struct bob_fbinfo*): switch to 640x480x32 graphics, map the framebuffer; 0 / -1 */
#define SYS_fb_exit 202  /* fb_exit(): back to text mode */
#define SYS_stat    106
#define SYS_clock_gettime 265 /* clock_gettime(clk, struct bob_timespec*): clk 0 = wall (utc), 1 = since boot */  /* stat(path, struct bob_stat*) */

#ifndef __ASSEMBLER__
struct bob_fbinfo {
    unsigned int width, height, pitch, bpp;
    unsigned int addr;       /* user virtual address of the pixels (0xAARRGGBB little endian) */
};
struct bob_clone {
    unsigned int fn;         /* void fn(void* arg) */
    unsigned int arg;
    unsigned int stack_top;  /* thread stack, grows down */
    unsigned int exit_stub;  /* fn's return address (should call thread_exit) */
};

struct bob_timespec {
    int tv_sec;
    int tv_nsec;
};

struct bob_stat {
    unsigned int size;
    unsigned int is_dir;
};
#endif

#ifndef __ASSEMBLER__
static inline int __syscall3(int num, int a, int b, int c) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return ret;
}
#endif

#endif
