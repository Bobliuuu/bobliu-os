#ifndef _DIRENT_H
#define _DIRENT_H 1

#define DIRENT_NAME_MAX 128

#ifdef __cplusplus
extern "C" {
#endif

/* name of the index-th entry of directory path: 1 = ok, 0 = no more, -1 = error */
int readdir_at(const char* path, int index, char name[DIRENT_NAME_MAX]);

#ifdef __cplusplus
}
#endif

#endif
