#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int write(int fd, const void* buf, size_t count);
int getpid(void);
__attribute__((__noreturn__)) void _exit(int status);

#ifdef __cplusplus
}
#endif

#endif
