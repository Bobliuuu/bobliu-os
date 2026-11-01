#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

static const char* g_base = "/tmp";

// base directory + "/" + name (a few rotating buffers so calls can be nested)
static char* P(const char* name) {
    static char bufs[4][64];
    static int i;
    char* b = bufs[i++ & 3];
    sprintf(b, "%s/%s", g_base, name);
    return b;
}

static int fail(const char* what) {
    printf("wtest FAIL: %s\n", what);
    return 1;
}

int main(int argc, char** argv) {
    if (argc > 1) g_base = argv[1];            // wtest /disk runs the same checks on the disk filesystem
    char buf[64];
    struct stat st;

    // write + read back
    int fd = open(P("a.txt"), O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return fail("create");
    if (write(fd, "hello ", 6) != 6 || write(fd, "tmpfs", 5) != 5) return fail("write");
    close(fd);
    if (stat(P("a.txt"), &st) < 0 || st.st_size != 11) return fail("stat size");
    fd = open(P("a.txt"), O_RDONLY);
    memset(buf, 0, sizeof(buf));
    if (read(fd, buf, 63) != 11 || strcmp(buf, "hello tmpfs") != 0) return fail("read back");
    if (write(fd, "x", 1) != -1) return fail("write on read-only fd");
    close(fd);
    printf("write/read: ok\n");

    // append, then truncate
    fd = open(P("a.txt"), O_WRONLY | O_APPEND);
    write(fd, "!!", 2);
    close(fd);
    stat(P("a.txt"), &st);
    if (st.st_size != 13) return fail("append");
    fd = open(P("a.txt"), O_WRONLY | O_TRUNC);
    close(fd);
    stat(P("a.txt"), &st);
    if (st.st_size != 0) return fail("truncate");
    printf("append/truncate: ok\n");

    // initrd stays read-only
    if (open("/hello.txt", O_WRONLY) >= 0) return fail("initrd writable");
    if (open("/newfile", O_WRONLY | O_CREAT) >= 0) return fail("create on initrd");
    if (unlink("/hello.txt") != -1) return fail("unlink on initrd");
    printf("initrd read-only: ok\n");

    // dirs
    if (mkdir(P("d")) < 0 || mkdir(P("d")) != -1) return fail("mkdir");
    fd = open(P("d/inner"), O_WRONLY | O_CREAT);
    if (fd < 0) return fail("create in subdir");
    write(fd, "in", 2);
    close(fd);
    if (unlink(P("d")) != -1) return fail("rmdir non-empty");
    if (unlink(P("d/inner")) != 0 || unlink(P("d")) != 0) return fail("remove");
    if (stat(P("d"), &st) == 0) return fail("still there");
    printf("mkdir/unlink: ok\n");

    // unlink an open file: still readable until closed
    fd = open(P("gone"), O_RDWR | O_CREAT);
    write(fd, "abc", 3);
    unlink(P("gone"));
    lseek(fd, 0, SEEK_SET);
    if (read(fd, buf, 3) != 3 || buf[0] != 'a') return fail("read unlinked");
    close(fd);
    printf("unlink while open: ok\n");

    // child writes, parent sees it (shared fs); shared fd offset
    fd = open(P("shared"), O_RDWR | O_CREAT);
    int pid = fork();
    if (pid == 0) { write(fd, "child ", 6); exit(0); }
    int s;
    waitpid(pid, &s, 0);
    write(fd, "parent", 6);
    lseek(fd, 0, SEEK_SET);
    memset(buf, 0, sizeof(buf));
    read(fd, buf, 20);
    if (strcmp(buf, "child parent") != 0) return fail("shared fd write");
    close(fd);
    unlink(P("shared"));
    unlink(P("a.txt"));
    printf("fork + write: ok\n");

    // churn: create/write/delete many files, memory must not run out
    for (int i = 0; i < 300; i++) {
        char name[64];
        sprintf(name, "%s/f%d", g_base, i);
        fd = open(name, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) return fail("churn create");
        static char block[4096];
        write(fd, block, sizeof(block));
        close(fd);
        unlink(name);
    }
    printf("churn: ok\n");
    printf("wtest done\n");
    return 0;
}
