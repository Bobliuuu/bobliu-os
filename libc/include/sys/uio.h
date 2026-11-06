#ifndef _SYS_UIO_H
#define _SYS_UIO_H 1
#include <sys/types.h>
struct iovec { void* iov_base; size_t iov_len; };
#ifdef __cplusplus
extern "C" {
#endif
ssize_t readv(int fd, const struct iovec* iov, int cnt);    /* emulated with read/write */
ssize_t writev(int fd, const struct iovec* iov, int cnt);
#ifdef __cplusplus
}
#endif
#endif
