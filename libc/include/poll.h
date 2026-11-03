#ifndef _POLL_H
#define _POLL_H 1
#ifdef __cplusplus
extern "C" {
#endif
struct pollfd { int fd; short events, revents; };
typedef unsigned int nfds_t;
#define POLLIN   0x001
#define POLLPRI  0x002
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020
#define POLLRDNORM POLLIN
#define POLLWRNORM POLLOUT
int poll(struct pollfd* fds, nfds_t n, int timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
