#ifndef _SCHED_H
#define _SCHED_H 1
struct sched_param { int sched_priority; };
#define SCHED_OTHER 0
#ifdef __cplusplus
extern "C" {
#endif
int sched_yield(void);
int sched_get_priority_min(int policy);
int sched_get_priority_max(int policy);
#ifdef __cplusplus
}
#endif
#endif
