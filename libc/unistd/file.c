#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <kernel/syscall.h>

int open(const char* path, int flags, ...) {
    return __syscall3(SYS_open, (int)path, flags, 0);
}

int unlink(const char* path) {
    return __syscall3(SYS_unlink, (int)path, 0, 0);
}

int mkdir(const char* path) {
    return __syscall3(SYS_mkdir, (int)path, 0, 0);
}

int close(int fd) {
    return __syscall3(SYS_close, fd, 0, 0);
}

int lseek(int fd, int offset, int whence) {
    return __syscall3(SYS_lseek, fd, offset, whence);
}

int stat(const char* path, struct stat* st) {
    return __syscall3(SYS_stat, (int)path, (int)st, 0);
}

int readdir_at(const char* path, int index, char name[DIRENT_NAME_MAX]) {
    return __syscall3(SYS_readdir, (int)path, index, (int)name);
}

int dup(int fd) {
    return __syscall3(SYS_dup, fd, 0, 0);
}

int dup2(int oldfd, int newfd) {
    return __syscall3(SYS_dup2, oldfd, newfd, 0);
}

int chdir(const char* path) {
    return __syscall3(SYS_chdir, (int)path, 0, 0);
}

char* getcwd(char* buf, size_t size) {
    return __syscall3(SYS_getcwd, (int)buf, (int)size, 0) < 0 ? 0 : buf;
}

int pipe(int fds[2]) {
    return __syscall3(SYS_pipe, (int)fds, 0, 0);
}
