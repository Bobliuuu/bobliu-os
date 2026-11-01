#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int fail(const char* what) {
    printf("fdtest FAIL: %s\n", what);
    return 1;
}

int main(void) {
    char a[8], b[8], c[8];
    int fd = open("/hello.txt", 0);
    if (fd < 3) return fail("open");

    int d = dup(fd);
    if (d < 0 || d == fd) return fail("dup");
    memset(a, 0, 8); memset(b, 0, 8);
    read(fd, a, 3);
    read(d, b, 3);                       // same open file: continues where fd stopped
    char two[8];
    lseek(fd, 0, SEEK_SET);
    read(fd, two, 6);
    two[6] = 0;
    if (memcmp(a, two, 3) != 0 || memcmp(b, two + 3, 3) != 0) return fail("dup shares offset");
    printf("dup shares offset: ok\n");

    // fork: child and parent share the offset too
    lseek(fd, 0, SEEK_SET);
    int pid = fork();
    if (pid == 0) {
        read(fd, c, 2);
        exit(0);
    }
    int st;
    waitpid(pid, &st, 0);
    read(fd, c, 2);
    c[2] = 0;
    if (memcmp(c, two + 2, 2) != 0) return fail("fork shares offset");
    printf("fork inherits fds: ok\n");

    // dup2 onto a high fd, stdout alias
    if (dup2(1, 9) != 9) return fail("dup2");
    write(9, "dup2 to console: ok\n", 20);

    // fd 1 can itself be closed and restored
    int saved = dup(1);
    close(1);
    if (write(1, "x", 1) != -1) return fail("write on closed fd");
    dup2(saved, 1);
    close(saved);
    printf("restore stdout: ok\n");

    // closing one ref leaves the other usable; double close fails
    if (close(fd) != 0) return fail("close");
    if (close(fd) != -1) return fail("double close");
    lseek(d, 0, SEEK_SET);
    if (read(d, a, 3) != 3) return fail("read after sibling close");
    close(d);
    close(9);

    // no leak: 40 children each holding the file open
    for (int i = 0; i < 40; i++) {
        int f = open("/hello.txt", 0);
        if (f < 0) return fail("fd leak (open)");
        int p = fork();
        if (p == 0) exit(0);
        waitpid(p, &st, 0);
        close(f);
    }
    printf("no fd leak: ok\n");
    printf("fdtest done\n");
    return 0;
}
