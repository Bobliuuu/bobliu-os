#ifndef _PTY_H
#define _PTY_H 1
#include <termios.h>
#include <sys/ioctl.h>
#ifdef __cplusplus
extern "C" {
#endif
int openpty(int* amaster, int* aslave, char* name, const struct termios* t, const struct winsize* w);   /* no ptys: fails with ENOSYS */
int forkpty(int* amaster, char* name, const struct termios* t, const struct winsize* w);
#ifdef __cplusplus
}
#endif
#endif
