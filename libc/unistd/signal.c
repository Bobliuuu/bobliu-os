#include <signal.h>
#include <kernel/syscall.h>

// handlers return here (the kernel put its address on the user stack): undo the signal frame
void __sigtramp(void);
__asm__(".global __sigtramp\n"
        "__sigtramp:\n"
        "    movl $119, %eax\n"          /* SYS_sigreturn */
        "    int $0x80\n");

sighandler_t signal(int sig, sighandler_t handler) {
    int old = __syscall3(SYS_signal, sig, (int)handler, (int)__sigtramp);
    return old == -1 ? SIG_ERR : (sighandler_t)old;
}

int kill(int pid, int sig) {
    return __syscall3(SYS_kill, pid, sig, 0);
}
