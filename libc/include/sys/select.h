#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H 1
#include <sys/types.h>
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FD_SETSIZE 64
typedef struct { unsigned long long bits; } fd_set;
#define FD_ZERO(s)   ((s)->bits = 0)
#define FD_SET(f, s) ((s)->bits |= 1ull << (f))
#define FD_CLR(f, s) ((s)->bits &= ~(1ull << (f)))
#define FD_ISSET(f, s) (((s)->bits >> (f)) & 1)
#include <sys/time.h>
int select(int nfds, fd_set* r, fd_set* w, fd_set* e, struct timeval* tv);
#ifdef __cplusplus
}
#endif
#endif
