#ifndef _TERMIOS_H
#define _TERMIOS_H 1
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;
#define NCCS 32
#define _POSIX_VDISABLE 0
struct termios {
    tcflag_t c_iflag, c_oflag, c_cflag, c_lflag;
    cc_t c_cc[NCCS];
};
/* c_lflag */
#define ISIG   0x01
#define ICANON 0x02
#define ECHO   0x08
#define ECHOE  0x10
#define ECHOK  0x20
#define ECHONL 0x40
#define NOFLSH 0x80
#define IEXTEN 0x8000
/* c_iflag */
#define IGNBRK 0x001
#define BRKINT 0x002
#define IGNPAR 0x004
#define PARMRK 0x008
#define INPCK  0x010
#define ISTRIP 0x020
#define INLCR  0x040
#define IGNCR  0x080
#define ICRNL  0x100
#define IXON   0x400
#define IXOFF  0x1000
/* c_oflag */
#define OPOST  0x01
#define ONLCR  0x04
/* c_cflag */
#define CSIZE  0x30
#define CS8    0x30
#define CREAD  0x80
#define PARENB 0x100
/* c_cc */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VEOL 11
#define VEOL2 16
#define VWERASE 14
#define VREPRINT 12
#define VLNEXT 15
#define VDISCARD 13
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define B9600 13
#define B38400 15
int tcgetattr(int fd, struct termios* t);
int tcsetattr(int fd, int actions, const struct termios* t);
void cfmakeraw(struct termios* t);
speed_t cfgetispeed(const struct termios* t);
speed_t cfgetospeed(const struct termios* t);
int cfsetispeed(struct termios* t, speed_t s);
int cfsetospeed(struct termios* t, speed_t s);
int tcflush(int fd, int queue);
int tcdrain(int fd);
int tcflow(int fd, int action);
int tcsendbreak(int fd, int duration);
#ifdef __cplusplus
}
#endif
#endif
