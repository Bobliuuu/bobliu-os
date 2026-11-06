#ifndef _DIRENT_H
#define _DIRENT_H 1

#define DIRENT_NAME_MAX 128

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
struct dirent { ino_t d_ino; unsigned char d_type; char d_name[DIRENT_NAME_MAX]; };
typedef struct _DIR DIR;
DIR* opendir(const char* path);
struct dirent* readdir(DIR* d);
int closedir(DIR* d);
void rewinddir(DIR* d);
int dirfd(DIR* d);
int scandir(const char* path, struct dirent*** list, int (*filter)(const struct dirent*), int (*cmp)(const struct dirent**, const struct dirent**));
int alphasort(const struct dirent** a, const struct dirent** b);

/* name of the index-th entry of directory path: 1 = ok, 0 = no more, -1 = error */
int readdir_at(const char* path, int index, char name[DIRENT_NAME_MAX]);

#ifdef __cplusplus
}
#endif

#endif
