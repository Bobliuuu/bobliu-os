#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1

struct stat {
    unsigned int st_size;
    unsigned int st_isdir;
};

#ifdef __cplusplus
extern "C" {
#endif

int stat(const char* path, struct stat* st);
int mkdir(const char* path);   /* only on writable mounts (/tmp) */

#ifdef __cplusplus
}
#endif

#endif
