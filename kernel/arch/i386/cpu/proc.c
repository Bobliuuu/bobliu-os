// process table, cooperative+timer round robin scheduler, fork/exec/wait/exit
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <kernel/proc.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>
#include <arch/i386/pmm.h>
#include <arch/i386/paging.h>
#include <arch/i386/tss.h>
#include <arch/i386/irq.h>

extern void swtch(struct context** old, struct context* new);
extern void trapret(void);

static proc_t g_procs[NPROC];
static proc_t* g_cur = 0;
static int g_next_pid = 1;

#define KERNEL_PROC (&g_procs[0])

void proc_init(void) {
    memset(g_procs, 0, sizeof(g_procs));
    proc_t* k = KERNEL_PROC;
    for (int f = 0; f < PROC_MAXFD; f++) k->fds[f] = f < 3 ? FD_CONSOLE : -1;
    k->pid = 0;
    k->state = PROC_RUNNING;
    k->pd = paging_kernel_directory();
    strcpy(k->name, "kernel");
    g_cur = k;
}

proc_t* proc_current(void) { return g_cur; }

static proc_t* proc_alloc(void) {
    for (int i = 1; i < NPROC; i++) {
        proc_t* p = &g_procs[i];
        if (p->state != PROC_UNUSED) continue;

        uint8_t* ks = (uint8_t*)kstack_alloc((uint32_t)i);
        if (!ks) return 0;

        memset(p, 0, sizeof(*p));
        // children share the parent's open files (same offset, like unix)
        for (int f = 0; f < PROC_MAXFD; f++) {
            p->fds[f] = g_cur->fds[f];
            if (p->fds[f] >= 0) vfs_dup(p->fds[f]);
        }
        p->pid = g_next_pid++;
        p->kstack = ks;
        p->parent = g_cur;

        // lay out: [ ... | context | trap frame ] growing down from the top
        uint8_t* top = ks + KSTACK_SIZE;
        p->tf = (regs_t*)(top - sizeof(regs_t));
        memset(p->tf, 0, sizeof(regs_t));
        p->ctx = (struct context*)((uint8_t*)p->tf - sizeof(struct context));
        memset(p->ctx, 0, sizeof(struct context));
        p->ctx->eip = (uint32_t)trapret;
        return p;
    }
    return 0;
}

static void fds_close_all(proc_t* p) {
    for (int i = 0; i < PROC_MAXFD; i++) {
        if (p->fds[i] >= 0) vfs_close(p->fds[i]);
        p->fds[i] = -1;
    }
}

static void proc_release(proc_t* p) {
    fds_close_all(p);
    kstack_free((uint32_t)(p - g_procs));
    p->kstack = 0;
    p->state = PROC_UNUSED;
    p->pid = 0;
}

// pick the next runnable proc after cur (round robin); 0 if none
static proc_t* pick_next(void) {
    int start = (int)(g_cur - g_procs);
    for (int i = 1; i <= NPROC; i++) {
        proc_t* p = &g_procs[(start + i) % NPROC];
        if (p->state == PROC_RUNNABLE) return p;
    }
    return 0;
}

// give up the cpu. caller sets g_cur->state first (RUNNABLE to just yield).
static void sched(void) {
    uint32_t fl = irq_save();

    proc_t* old = g_cur;
    proc_t* nxt;
    while (!(nxt = pick_next())) {
        if (old->state == PROC_RUNNING) { irq_restore(fl); return; }
        // nothing to run (e.g. everybody asleep): wait for an interrupt
        __asm__ volatile("sti; hlt; cli");
    }

    if (nxt == old) {           // e.g. woken from sleep and nothing else is runnable
        old->state = PROC_RUNNING;
        irq_restore(fl);
        return;
    }

    if (old->state == PROC_RUNNING) old->state = PROC_RUNNABLE;
    nxt->state = PROC_RUNNING;
    g_cur = nxt;

    if (nxt->kstack) tss_set_kernel_stack((uint32_t)(nxt->kstack + KSTACK_SIZE));
    paging_load(nxt->pd);
    swtch(&old->ctx, nxt->ctx);

    // back in `old` after somebody switched to us again
    irq_restore(fl);
}

void proc_yield(void) {
    g_cur->state = PROC_RUNNING;   // sched() turns it into RUNNABLE
    sched();
}

static void setup_user_frame(regs_t* tf, uint32_t entry, uint32_t sp) {
    memset(tf, 0, sizeof(*tf));
    tf->ds = 0x23;
    tf->cs = 0x1B;
    tf->ss = 0x23;
    tf->eip = entry;
    tf->useresp = sp;
    tf->eflags = 0x202;      // IF=1
}

int proc_spawn(const char* path, char** argv, int argc) {
    page_directory_t pd;
    uint32_t entry, sp;
    if (user_load(path, argv, argc, &pd, &entry, &sp) < 0) return -1;

    proc_t* p = proc_alloc();
    if (!p) { paging_free_user(pd, 1); return -1; }

    p->pd = pd;
    p->brk = USER_HEAP_BASE;
    setup_user_frame(p->tf, entry, sp);
    strncpy(p->name, path, sizeof(p->name) - 1);

    uint32_t fl = irq_save();
    p->state = PROC_RUNNABLE;
    irq_restore(fl);
    return p->pid;
}

int proc_fork(regs_t* tf) {
    proc_t* parent = g_cur;
    proc_t* c = proc_alloc();
    if (!c) return -1;

    c->pd = paging_clone_user(parent->pd);
    if (!c->pd.pd_phys) { proc_release(c); return -1; }

    memcpy(c->tf, tf, sizeof(regs_t));
    c->tf->eax = 0;                       // child sees fork() == 0
    c->brk = parent->brk;                 // heap pages came along (cow)
    memcpy(c->name, parent->name, sizeof(c->name));

    uint32_t fl = irq_save();
    c->state = PROC_RUNNABLE;
    irq_restore(fl);
    return c->pid;
}

int proc_exec(const char* path, char** argv, int argc, regs_t* tf) {
    page_directory_t npd;
    uint32_t entry, sp;
    if (user_load(path, argv, argc, &npd, &entry, &sp) < 0) return -1;

    proc_t* p = g_cur;
    page_directory_t old = p->pd;

    p->pd = npd;
    p->brk = USER_HEAP_BASE;
    paging_load(npd);
    paging_free_user(old, 1);

    uint32_t eflags = tf->eflags;
    setup_user_frame(tf, entry, sp);
    tf->eflags = eflags | 0x200;
    strncpy(p->name, path, sizeof(p->name) - 1);
    return 0;
}

void proc_exit(int code) {
    proc_t* p = g_cur;
    if (p == KERNEL_PROC) { printf("proc_exit: kernel cannot exit\n"); for (;;) __asm__ volatile("hlt"); }

    fds_close_all(p);

    uint32_t fl = irq_save();   // never restored: this context dies

    // drop the address space (we are on our kernel stack, kernel pd is safe to use)
    page_directory_t dead = p->pd;
    p->pd = paging_kernel_directory();
    paging_load(p->pd);
    paging_free_user(dead, 1);

    // children get adopted by the kernel context
    for (int i = 1; i < NPROC; i++) {
        if (g_procs[i].state != PROC_UNUSED && g_procs[i].parent == p)
            g_procs[i].parent = KERNEL_PROC;
    }

    p->exit_code = code;
    p->state = PROC_ZOMBIE;
    if (p->parent && p->parent->state == PROC_SLEEPING && p->parent->chan == WAIT_CHILD)
        p->parent->state = PROC_RUNNABLE;

    sched();
    (void)fl;
    printf("proc_exit: zombie scheduled again?!\n");
    for (;;) __asm__ volatile("cli; hlt");
}

int proc_wait(int pid, int* status) {
    proc_t* me = g_cur;
    for (;;) {
        int have_child = 0;
        for (int i = 1; i < NPROC; i++) {
            proc_t* c = &g_procs[i];
            if (c->state == PROC_UNUSED || c->parent != me) continue;
            if (pid > 0 && c->pid != pid) continue;
            have_child = 1;
            if (c->state == PROC_ZOMBIE) {
                int cpid = c->pid;
                if (status) *status = (c->exit_code & 0xFF) << 8;
                uint32_t fl = irq_save();
                proc_release(c);
                irq_restore(fl);
                return cpid;
            }
        }
        if (!have_child) return -1;

        uint32_t fl = irq_save();
        me->chan = WAIT_CHILD;
        me->state = PROC_SLEEPING;
        sched();
        irq_restore(fl);
    }
}

void proc_sleep_on(int chan) {
    g_cur->chan = chan;
    g_cur->state = PROC_SLEEPING;
    sched();
}

void proc_wakeup(int chan) {
    for (int i = 0; i < NPROC; i++) {
        proc_t* p = &g_procs[i];
        if (p->state == PROC_SLEEPING && p->chan == chan) p->state = PROC_RUNNABLE;
    }
}

int proc_kernel_busy(void) {
    return KERNEL_PROC->state == PROC_SLEEPING;
}

void proc_reap_orphans(void) {
    for (int i = 1; i < NPROC; i++) {
        proc_t* c = &g_procs[i];
        if (c->state == PROC_ZOMBIE && c->parent == KERNEL_PROC) {
            uint32_t fl = irq_save();
            proc_release(c);
            irq_restore(fl);
        }
    }
}

// entry for a user fd: vfs fd (>=0), FD_CONSOLE, or -1 if not open
int proc_fd_get(int ufd) {
    if (ufd < 0 || ufd >= PROC_MAXFD) return -1;
    return g_cur->fds[ufd] == -1 ? -1 : g_cur->fds[ufd];
}

// lowest free user fd holding `entry`, or -1
int proc_fd_alloc(int entry) {
    for (int i = 0; i < PROC_MAXFD; i++) {
        if (g_cur->fds[i] == -1) { g_cur->fds[i] = entry; return i; }
    }
    return -1;
}

int proc_fd_close(int ufd) {
    if (ufd < 0 || ufd >= PROC_MAXFD || g_cur->fds[ufd] == -1) return -1;
    if (g_cur->fds[ufd] >= 0) vfs_close(g_cur->fds[ufd]);
    g_cur->fds[ufd] = -1;
    return 0;
}

// dup2: make `nfd` refer to the same open file as `ofd` (nfd < 0: lowest free)
int proc_fd_dup(int ofd, int nfd) {
    int e = proc_fd_get(ofd);
    if (e == -1) return -1;
    if (nfd == ofd) return nfd;
    if (nfd < 0) {
        for (nfd = 0; nfd < PROC_MAXFD && g_cur->fds[nfd] != -1; nfd++);
        if (nfd >= PROC_MAXFD) return -1;
    } else {
        if (nfd >= PROC_MAXFD) return -1;
        proc_fd_close(nfd);
    }
    if (e >= 0) vfs_dup(e);
    g_cur->fds[nfd] = e;
    return nfd;
}

int proc_brk(uint32_t addr) {
    proc_t* p = g_cur;
    if (addr == 0) return (int)p->brk;
    if (addr < USER_HEAP_BASE || addr > USER_HEAP_MAX) return -1;

    uint32_t cur = (p->brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t top = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint32_t va = cur; va < top; va += PAGE_SIZE) {
        uint32_t f = (uint32_t)pmm_alloc_frame();
        if (!f || paging_map_in(p->pd, va, f, P_USER | P_RW) < 0) {
            if (f) pmm_free_frame(f);
            for (uint32_t u = cur; u < va; u += PAGE_SIZE) paging_unmap_in(p->pd, u);
            return -1;
        }
        memset((void*)f, 0, PAGE_SIZE);        // identity mapped
    }
    for (uint32_t va = top; va < cur; va += PAGE_SIZE) paging_unmap_in(p->pd, va);

    p->brk = addr;
    return (int)addr;
}

int proc_count(void) {
    int n = 0;
    for (int i = 0; i < NPROC; i++) if (g_procs[i].state != PROC_UNUSED) n++;
    return n;
}

void proc_dump(void) {
    static const char* names[] = { "unused", "runnable", "running", "sleeping", "zombie" };
    printf("PID PPID STATE    NAME\n");
    for (int i = 0; i < NPROC; i++) {
        proc_t* p = &g_procs[i];
        if (p->state == PROC_UNUSED) continue;
        printf("%d   %d    %s  %s\n", p->pid, p->parent ? p->parent->pid : 0,
               names[p->state], p->name);
    }
}
