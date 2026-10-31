// process table, cooperative+timer round robin scheduler, fork/exec/wait/exit
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <kernel/proc.h>
#include <kernel/heap.h>
#include <arch/i386/paging.h>
#include <arch/i386/tss.h>

extern void swtch(struct context** old, struct context* new);
extern void trapret(void);

static proc_t g_procs[NPROC];
static proc_t* g_cur = 0;
static int g_next_pid = 1;

#define KERNEL_PROC (&g_procs[0])

static inline uint32_t irq_save(void) {
    uint32_t f;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(f) :: "memory");
    return f;
}
static inline void irq_restore(uint32_t f) {
    __asm__ volatile("pushl %0; popfl" :: "r"(f) : "memory", "cc");
}

void proc_init(void) {
    memset(g_procs, 0, sizeof(g_procs));
    proc_t* k = KERNEL_PROC;
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

        uint8_t* ks = (uint8_t*)kmalloc(KSTACK_SIZE);
        if (!ks) return 0;

        memset(p, 0, sizeof(*p));
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

static void proc_release(proc_t* p) {
    kfree(p->kstack);
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

static void setup_user_frame(regs_t* tf, uint32_t entry) {
    memset(tf, 0, sizeof(*tf));
    tf->ds = 0x23;
    tf->cs = 0x1B;
    tf->ss = 0x23;
    tf->eip = entry;
    tf->useresp = USER_STACK_TOP;
    tf->eflags = 0x202;      // IF=1
}

int proc_spawn(const char* path) {
    page_directory_t pd;
    uint32_t entry;
    if (user_load(path, &pd, &entry) < 0) return -1;

    proc_t* p = proc_alloc();
    if (!p) { paging_free_user(pd, 1); return -1; }

    p->pd = pd;
    setup_user_frame(p->tf, entry);
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
    memcpy(c->name, parent->name, sizeof(c->name));

    uint32_t fl = irq_save();
    c->state = PROC_RUNNABLE;
    irq_restore(fl);
    return c->pid;
}

int proc_exec(const char* path, regs_t* tf) {
    page_directory_t npd;
    uint32_t entry;
    if (user_load(path, &npd, &entry) < 0) return -1;

    proc_t* p = g_cur;
    page_directory_t old = p->pd;

    p->pd = npd;
    paging_load(npd);
    paging_free_user(old, 1);

    uint32_t eflags = tf->eflags;
    setup_user_frame(tf, entry);
    tf->eflags = eflags | 0x200;
    strncpy(p->name, path, sizeof(p->name) - 1);
    return 0;
}

void proc_exit(int code) {
    proc_t* p = g_cur;
    if (p == KERNEL_PROC) { printf("proc_exit: kernel cannot exit\n"); for (;;) __asm__ volatile("hlt"); }

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
    if (p->parent && p->parent->state == PROC_SLEEPING) p->parent->state = PROC_RUNNABLE;

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
        me->state = PROC_SLEEPING;
        sched();
        irq_restore(fl);
    }
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
