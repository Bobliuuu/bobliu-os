#include <poll.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>
#include <kernel/syscall.h>

int poll(struct pollfd* fds, nfds_t n, int timeout) {
    int r = __syscall3(SYS_poll, (int)fds, (int)n, timeout);
    if (r < 0) errno = EINTR;
    return r;
}

int select(int nfds, fd_set* rd, fd_set* wr, fd_set* ex, struct timeval* tv) {
    struct pollfd p[FD_SETSIZE];
    int n = 0;
    if (nfds > FD_SETSIZE) nfds = FD_SETSIZE;
    for (int f = 0; f < nfds; f++) {
        short ev = 0;
        if (rd && FD_ISSET(f, rd)) ev |= POLLIN;
        if (wr && FD_ISSET(f, wr)) ev |= POLLOUT;
        if (ex && FD_ISSET(f, ex)) ev |= POLLPRI;
        if (ev) { p[n].fd = f; p[n].events = ev; p[n].revents = 0; n++; }
    }
    int timeout = tv ? (int)(tv->tv_sec * 1000 + (tv->tv_usec + 999) / 1000) : -1;
    int r = poll(p, (nfds_t)n, timeout);
    if (r < 0) return -1;
    if (rd) FD_ZERO(rd);
    if (wr) FD_ZERO(wr);
    if (ex) FD_ZERO(ex);
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        if (rd && (p[i].revents & (POLLIN | POLLHUP | POLLERR))) { FD_SET(p[i].fd, rd); cnt++; }
        if (wr && (p[i].revents & (POLLOUT | POLLERR))) { FD_SET(p[i].fd, wr); cnt++; }
    }
    return cnt;
}

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);
    int arg = va_arg(ap, int);
    va_end(ap);
    int r = __syscall3(SYS_fcntl, fd, cmd, arg);
    if (r < 0) errno = EBADF;
    return r;
}

int pipe2(int fds[2], int flags) {
    if (pipe(fds) < 0) return -1;
    if (flags & O_NONBLOCK) { fcntl(fds[0], F_SETFL, O_NONBLOCK); fcntl(fds[1], F_SETFL, O_NONBLOCK); }
    return 0;
}

#include <time.h>
int gettimeofday(struct timeval* tv, struct timezone* tz) {
    (void)tz;
    struct timespec ts;
    if (clock_gettime(0, &ts) < 0) return -1;
    if (tv) { tv->tv_sec = ts.tv_sec; tv->tv_usec = ts.tv_nsec / 1000; }
    return 0;
}
