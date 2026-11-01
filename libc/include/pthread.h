#ifndef _PTHREAD_H
#define _PTHREAD_H 1

/* a small pthread subset on top of clone/futex. threads share the whole address space, the
   fd table, cwd and heap with the process. there are no attributes, no cancellation, no TLS. */

typedef int pthread_t;
typedef struct { volatile int locked; } pthread_mutex_t;
typedef struct { volatile int seq; } pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER { 0 }
#define PTHREAD_COND_INITIALIZER  { 0 }
#define PTHREAD_STACK_SIZE 16384

#ifdef __cplusplus
extern "C" {
#endif

int  pthread_create(pthread_t* t, const void* attr, void* (*fn)(void*), void* arg);
int  pthread_join(pthread_t t, void** ret);
__attribute__((noreturn)) void pthread_exit(void* ret);   /* from the main thread this ends the process */
pthread_t pthread_self(void);

int  pthread_mutex_init(pthread_mutex_t* m, const void* attr);
int  pthread_mutex_lock(pthread_mutex_t* m);
int  pthread_mutex_trylock(pthread_mutex_t* m);
int  pthread_mutex_unlock(pthread_mutex_t* m);

int  pthread_cond_init(pthread_cond_t* c, const void* attr);
int  pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m);
int  pthread_cond_signal(pthread_cond_t* c);
int  pthread_cond_broadcast(pthread_cond_t* c);

int  sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif
