#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H 1
#include <pthread.h>
typedef struct { pthread_mutex_t m; pthread_cond_t c; int v; } sem_t;
#ifdef __cplusplus
extern "C" {
#endif
int sem_init(sem_t* s, int pshared, unsigned value);
int sem_destroy(sem_t* s);
int sem_post(sem_t* s);
int sem_wait(sem_t* s);
int sem_trywait(sem_t* s);
#ifdef __cplusplus
}
#endif
#endif
