#ifndef _SYS_STATFS_H
#define _SYS_STATFS_H 1
#include <sys/types.h>
struct statfs { long f_type, f_bsize; unsigned long f_blocks, f_bfree, f_bavail, f_files, f_ffree; long f_namelen, f_frsize, f_flags; };
int statfs(const char* path, struct statfs* s);
int fstatfs(int fd, struct statfs* s);
#endif
