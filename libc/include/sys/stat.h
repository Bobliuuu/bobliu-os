#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1
#include <sys/types.h>
#include <time.h>

struct stat {
    unsigned int st_size;        /* bobliu-native fields first (kept for old programs) */
    unsigned int st_isdir;
    unsigned int st_mode;
    unsigned int st_mtime;
    unsigned int st_ino;
    unsigned int st_nlink;       /* the kernel fills exactly the fields above */
    unsigned int st_dev, st_rdev, st_uid, st_gid, st_blksize, st_blocks, st_atime, st_ctime;
    struct timespec st_atim, st_mtim, st_ctim;
};

#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_IREAD S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC S_IXUSR
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 0070
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXO 0007
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#ifdef __cplusplus
extern "C" {
#endif

int stat(const char* path, struct stat* st);
int lstat(const char* path, struct stat* st);   /* no symlinks: same as stat */
int fstat(int fd, struct stat* st);
int mkdir(const char* path, ...);               /* the mode argument is accepted and ignored */
int chmod(const char* path, mode_t mode);       /* accepted, no effect */
int fchmod(int fd, mode_t mode);
mode_t umask(mode_t m);
int utimensat(int dirfd, const char* path, const struct timespec ts[2], int flags);
int futimens(int fd, const struct timespec ts[2]);   /* accepted, no effect: files have no stored times */
int utimes(const char* path, const void* tv);
int futimes(int fd, const void* tv);

#ifdef __cplusplus
}
#endif

#endif
