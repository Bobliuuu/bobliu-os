#ifndef _PTHREAD_H
#define _PTHREAD_H 1

/* a small pthread subset on top of clone/futex. threads share the whole address space, the
   fd table, cwd and heap with the process. there are no attributes, no cancellation, no TLS. */

typedef int pthread_t;
typedef struct { unsigned stacksize; int detach; } pthread_attr_t;
typedef struct { int type; } pthread_mutexattr_t;
typedef struct { int dummy; } pthread_condattr_t;
typedef int pthread_key_t;
typedef struct { volatile int state; } pthread_once_t;
typedef struct { int readers; int writer; volatile int lock; } pthread_rwlock_t;
typedef struct { volatile int locked; } pthread_mutex_t;
typedef struct { volatile int seq; } pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER { 0 }
#define PTHREAD_COND_INITIALIZER  { 0 }
#define PTHREAD_STACK_SIZE 16384
#define PTHREAD_ONCE_INIT { 0 }
#define PTHREAD_RWLOCK_INITIALIZER { 0, 0, 0 }
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT 0
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

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

int  pthread_mutex_destroy(pthread_mutex_t* m);
int  pthread_cond_destroy(pthread_cond_t* c);
struct timespec;
int  pthread_cond_timedwait(pthread_cond_t* c, pthread_mutex_t* m, const struct timespec* abs);
int  pthread_mutexattr_init(pthread_mutexattr_t* a);
int  pthread_mutexattr_destroy(pthread_mutexattr_t* a);
int  pthread_mutexattr_settype(pthread_mutexattr_t* a, int type);
int  pthread_attr_init(pthread_attr_t* a);
int  pthread_attr_destroy(pthread_attr_t* a);
int  pthread_attr_setstacksize(pthread_attr_t* a, unsigned n);
int  pthread_attr_getstacksize(const pthread_attr_t* a, unsigned* n);
int  pthread_equal(pthread_t a, pthread_t b);
int  pthread_once(pthread_once_t* once, void (*fn)(void));
int  pthread_key_create(pthread_key_t* k, void (*dtor)(void*));
int  pthread_key_delete(pthread_key_t k);
void* pthread_getspecific(pthread_key_t k);
int  pthread_setspecific(pthread_key_t k, const void* v);
int  pthread_rwlock_init(pthread_rwlock_t* l, const void* attr);
int  pthread_rwlock_destroy(pthread_rwlock_t* l);
int  pthread_rwlock_rdlock(pthread_rwlock_t* l);
int  pthread_rwlock_tryrdlock(pthread_rwlock_t* l);
int  pthread_rwlock_wrlock(pthread_rwlock_t* l);
int  pthread_rwlock_trywrlock(pthread_rwlock_t* l);
int  pthread_rwlock_unlock(pthread_rwlock_t* l);
int  pthread_detach(pthread_t t);

int  sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif
