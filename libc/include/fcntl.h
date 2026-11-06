#ifndef _FCNTL_H
#define _FCNTL_H 1

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR   0x002
#define O_CREAT  0x040
#define O_TRUNC  0x200
#define O_APPEND 0x400
#define O_NONBLOCK 0x800
#define O_CLOEXEC 0x80000
#define O_EXCL 0x80
#define O_NOCTTY 0x100
#define O_NOFOLLOW 0x20000
#define O_DIRECTORY 0x10000
#define O_SYNC 0x101000
#define O_DSYNC 0x1000
#define O_NDELAY O_NONBLOCK
#define F_DUPFD 0
#define F_DUPFD_CLOEXEC 1030
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define FD_CLOEXEC 1
#define O_ACCMODE 3
#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#ifdef __cplusplus
extern "C" {
#endif
int fcntl(int fd, int cmd, ...);
#ifdef __cplusplus
}
#endif

#endif
