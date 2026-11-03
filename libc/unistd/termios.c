#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <kernel/syscall.h>

int tcgetattr(int fd, struct termios* t) {
    int r = __syscall3(SYS_tcgetattr, fd, (int)t, 0);
    if (r < 0) errno = ENOTTY;
    return r;
}
int tcsetattr(int fd, int actions, const struct termios* t) {
    (void)actions;
    int r = __syscall3(SYS_tcsetattr, fd, (int)t, 0);
    if (r < 0) errno = ENOTTY;
    return r;
}
int isatty(int fd) {
    struct termios t;
    return __syscall3(SYS_tcgetattr, fd, (int)&t, 0) == 0;
}
void cfmakeraw(struct termios* t) {
    t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->c_oflag &= ~OPOST;
    t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->c_cflag &= ~(CSIZE | PARENB);
    t->c_cflag |= CS8;
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
}
speed_t cfgetispeed(const struct termios* t) { (void)t; return B38400; }
speed_t cfgetospeed(const struct termios* t) { (void)t; return B38400; }
int cfsetispeed(struct termios* t, speed_t s) { (void)t; (void)s; return 0; }
int cfsetospeed(struct termios* t, speed_t s) { (void)t; (void)s; return 0; }
int tcflush(int fd, int q) { (void)fd; (void)q; return 0; }
int tcdrain(int fd) { (void)fd; return 0; }
int tcflow(int fd, int a) { (void)fd; (void)a; return 0; }
int tcsendbreak(int fd, int d) { (void)fd; (void)d; return 0; }

int ioctl(int fd, unsigned long req, ...) {
    va_list ap;
    va_start(ap, req);
    void* arg = va_arg(ap, void*);
    va_end(ap);
    switch (req) {
    case TIOCGWINSZ: {
        if (!isatty(fd)) { errno = ENOTTY; return -1; }
        struct winsize* w = arg;
        w->ws_row = 25; w->ws_col = 80; w->ws_xpixel = w->ws_ypixel = 0;
        return 0;
    }
    case TCGETS:  return tcgetattr(fd, arg);
    case TCSETS: case TCSETSW: case TCSETSF: return tcsetattr(fd, 0, arg);
    }
    errno = EINVAL;
    return -1;
}
