#ifndef _SYS_FILE_H
#define _SYS_FILE_H 1
#include <fcntl.h>
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
#ifdef __cplusplus
extern "C" {
#endif
int flock(int fd, int op);   /* single user, no sharing: always succeeds */
#ifdef __cplusplus
}
#endif
#endif
