#include <pthread.h>
#include <stdlib.h>
#include <kernel/syscall.h>

#define MAX_THREADS 32

struct tstart { void* (*fn)(void*); void* arg; };

// tid -> malloc'd stack, so join can free it
static struct { int tid; void* stack; } g_stacks[MAX_THREADS];
static volatile int g_stack_lock;

static void table_lock(void)   { while (__sync_lock_test_and_set(&g_stack_lock, 1)) __syscall3(SYS_sched_yield, 0, 0, 0); }
static void table_unlock(void) { __sync_lock_release(&g_stack_lock); }

static void futex_wait(volatile int* addr, int val) { __syscall3(SYS_futex, (int)addr, FUTEX_WAIT, val); }
static void futex_wake(volatile int* addr)          { __syscall3(SYS_futex, (int)addr, FUTEX_WAKE, 0); }

static void thread_exit_stub(void) {
    __syscall3(SYS_thread_exit, 0, 0, 0);
    for (;;);
}

static void thread_main(void* p) {
    struct tstart s = *(struct tstart*)p;
    free(p);
    void* r = s.fn(s.arg);
    __syscall3(SYS_thread_exit, (int)r, 0, 0);
}

int pthread_create(pthread_t* t, const void* attr, void* (*fn)(void*), void* arg) {
    (void)attr;
    struct tstart* s = malloc(sizeof(*s));
    char* stack = malloc(PTHREAD_STACK_SIZE);
    if (!s || !stack) { free(s); free(stack); return -1; }
    s->fn = fn;
    s->arg = arg;

    struct bob_clone c = { (unsigned)thread_main, (unsigned)s, (unsigned)(stack + PTHREAD_STACK_SIZE), (unsigned)thread_exit_stub };
    int tid = __syscall3(SYS_clone, (int)&c, 0, 0);
    if (tid < 0) { free(s); free(stack); return -1; }

    table_lock();
    for (int i = 0; i < MAX_THREADS; i++) {
        if (!g_stacks[i].stack) { g_stacks[i].tid = tid; g_stacks[i].stack = stack; stack = 0; break; }
    }
    table_unlock();
    (void)stack;                           // table full: the stack leaks, the thread still works
    *t = tid;
    return 0;
}

int pthread_join(pthread_t t, void** ret) {
    int code = 0;
    if (__syscall3(SYS_thread_join, t, (int)&code, 0) < 0) return -1;
    if (ret) *ret = (void*)code;
    table_lock();
    for (int i = 0; i < MAX_THREADS; i++) {
        if (g_stacks[i].stack && g_stacks[i].tid == t) { free(g_stacks[i].stack); g_stacks[i].stack = 0; break; }
    }
    table_unlock();
    return 0;
}

void pthread_exit(void* ret) {
    __syscall3(SYS_thread_exit, (int)ret, 0, 0);
    for (;;);
}

pthread_t pthread_self(void) { return __syscall3(SYS_gettid, 0, 0, 0); }

int sched_yield(void) { return __syscall3(SYS_sched_yield, 0, 0, 0); }

int pthread_mutex_init(pthread_mutex_t* m, const void* attr) { (void)attr; m->locked = 0; return 0; }

int pthread_mutex_trylock(pthread_mutex_t* m) { return __sync_lock_test_and_set(&m->locked, 1) ? -1 : 0; }

int pthread_mutex_lock(pthread_mutex_t* m) {
    while (__sync_lock_test_and_set(&m->locked, 1)) futex_wait(&m->locked, 1);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* m) {
    __sync_lock_release(&m->locked);
    futex_wake(&m->locked);
    return 0;
}

int pthread_cond_init(pthread_cond_t* c, const void* attr) { (void)attr; c->seq = 0; return 0; }

int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m) {
    int seq = c->seq;
    pthread_mutex_unlock(m);
    futex_wait(&c->seq, seq);               // returns at once if a signal already bumped seq
    return pthread_mutex_lock(m);
}

int pthread_cond_signal(pthread_cond_t* c)    { __sync_fetch_and_add(&c->seq, 1); futex_wake(&c->seq); return 0; }
int pthread_cond_broadcast(pthread_cond_t* c) { return pthread_cond_signal(c); }
