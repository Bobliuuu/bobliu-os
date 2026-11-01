// signals: pending/mask bitmaps per process, delivered when a process is about to
// return to ring 3. a handler runs on the user stack above a frame holding the
// interrupted registers; its return address is a libc stub that calls sigreturn.
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <kernel/signal.h>
#include <kernel/proc.h>
#include <arch/i386/paging.h>
#include <arch/i386/irq.h>

struct sigframe {
    uint32_t ret;          // restorer: where the handler returns to
    uint32_t sig;          // handler's argument
    uint32_t oldmask;
    regs_t   saved;
};

static int user_writable(page_directory_t pd, uint32_t p, uint32_t len) {
    if (p < USER_LO || p >= USER_HI || len > USER_HI - p) return 0;
    for (uint32_t va = p & ~0xFFFu; va <= ((p + len - 1) & ~0xFFFu); va += PAGE_SIZE)
        if (!paging_translate_in(pd, va)) return 0;
    return 1;
}

int sig_send(proc_t* p, int sig) {
    if (sig < 0 || sig >= NSIG) return -1;
    if (!p || p->state == PROC_UNUSED || p->state == PROC_ZOMBIE) return -1;
    if (sig == 0) return 0;                              // just checking it exists

    uint32_t fl = irq_save();
    if (!(p->sig_handler[sig] == SIG_IGN && sig != SIGKILL)) {
        p->sig_pending |= 1u << sig;
        if (p->state == PROC_SLEEPING) p->state = PROC_RUNNABLE;   // let it notice
    }
    irq_restore(fl);
    return 0;
}

void sig_broadcast(int sig) {
    for (int i = 1; i < NPROC; i++) {
        proc_t* p = proc_by_slot(i);
        if (p && p->state != PROC_UNUSED && p->state != PROC_ZOMBIE) sig_send(p, sig);
    }
}

static uint32_t deliverable(proc_t* p) {
    uint32_t mask = p->sig_mask & ~(1u << SIGKILL);
    return p->sig_pending & ~mask;
}

int sig_interrupting(void) {
    return deliverable(proc_current()) != 0;
}

int sig_set(int sig, uint32_t handler, uint32_t restorer) {
    if (sig <= 0 || sig >= NSIG || sig == SIGKILL) return -1;
    proc_t* p = proc_current();
    uint32_t old = p->sig_handler[sig];
    p->sig_handler[sig] = handler;
    if (handler > SIG_IGN) p->sig_restorer = restorer;
    if (handler == SIG_IGN) p->sig_pending &= ~(1u << sig);
    return (int)old;
}

void sig_exec_reset(proc_t* p) {
    for (int i = 0; i < NSIG; i++)
        if (p->sig_handler[i] > SIG_IGN) p->sig_handler[i] = SIG_DFL;
    p->sig_restorer = 0;
    p->sig_mask = 0;
}

static void die(int sig) {
    printf("\n[pid %d killed by signal %d]\n", proc_current()->pid, sig);
    proc_exit(128 + sig);
}

void sig_deliver(regs_t* tf) {
    if ((tf->cs & 3) != 3) return;
    proc_t* p = proc_current();

    for (;;) {
        uint32_t ready = deliverable(p);
        if (!ready) return;
        int sig = __builtin_ctz(ready);
        p->sig_pending &= ~(1u << sig);

        uint32_t h = p->sig_handler[sig];
        if (sig == SIGKILL) die(sig);
        if (h == SIG_IGN) continue;
        if (h == SIG_DFL) die(sig);                       // every default action here is "terminate"

        uint32_t sp = (tf->useresp - sizeof(struct sigframe)) & ~3u;
        if (!p->sig_restorer || !user_writable(p->pd, sp, sizeof(struct sigframe))) die(SIGSEGV);

        struct sigframe* f = (struct sigframe*)sp;
        f->ret = p->sig_restorer;
        f->sig = (uint32_t)sig;
        f->oldmask = p->sig_mask;
        f->saved = *tf;

        p->sig_mask |= 1u << sig;                         // no recursion on the same signal
        tf->eip = h;
        tf->useresp = sp;
        return;                                           // one handler at a time
    }
}

void sig_return(regs_t* tf) {
    proc_t* p = proc_current();
    uint32_t sp = tf->useresp - 4;                        // the handler's `ret` popped f->ret
    if (!user_writable(p->pd, sp, sizeof(struct sigframe))) die(SIGSEGV);

    struct sigframe* f = (struct sigframe*)sp;
    regs_t s = f->saved;
    if (s.eip < USER_LO || s.eip >= USER_HI || s.useresp < USER_LO || s.useresp > USER_HI) die(SIGSEGV);

    tf->ds = 0x23; tf->cs = 0x1B; tf->ss = 0x23;          // never trust segment values from user memory
    tf->eflags = (s.eflags & 0xCD5) | 0x202;              // arithmetic flags only, IF on, IOPL 0
    tf->eip = s.eip; tf->useresp = s.useresp;
    tf->edi = s.edi; tf->esi = s.esi; tf->ebp = s.ebp; tf->ebx = s.ebx;
    tf->edx = s.edx; tf->ecx = s.ecx; tf->eax = s.eax;
    p->sig_mask = f->oldmask;
}
