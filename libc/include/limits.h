#ifndef _BOBLIU_LIMITS_H
#define _BOBLIU_LIMITS_H 1
#include_next <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 256
#endif
#define NAME_MAX 63
#define PIPE_BUF 4096
#define IOV_MAX 16
#define LINE_MAX 2048
#define NGROUPS_MAX 1
#define ARG_MAX 2048
#define OPEN_MAX 16
#define PTHREAD_STACK_MIN 16384
#define LOGIN_NAME_MAX 32
#define HOST_NAME_MAX 64
#define SSIZE_MAX 2147483647
#define MB_LEN_MAX 4
#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 256
#endif
#endif
