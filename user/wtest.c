#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

static int fail(const char* what) {
    printf("wtest FAIL: %s\n", what);
    return 1;
}

int main(void) {
    char buf[64];
    struct stat st;

    // write + read back
    int fd = open("/tmp/a.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return fail("create");
    if (write(fd, "hello ", 6) != 6 || write(fd, "tmpfs", 5) != 5) return fail("write");
    close(fd);
    if (stat("/tmp/a.txt", &st) < 0 || st.st_size != 11) return fail("stat size");
    fd = open("/tmp/a.txt", O_RDONLY);
    memset(buf, 0, sizeof(buf));
    if (read(fd, buf, 63) != 11 || strcmp(buf, "hello tmpfs") != 0) return fail("read back");
    if (write(fd, "x", 1) != -1) return fail("write on read-only fd");
    close(fd);
    printf("write/read: ok\n");

    // append, then truncate
    fd = open("/tmp/a.txt", O_WRONLY | O_APPEND);
    write(fd, "!!", 2);
    close(fd);
    stat("/tmp/a.txt", &st);
    if (st.st_size != 13) return fail("append");
    fd = open("/tmp/a.txt", O_WRONLY | O_TRUNC);
    close(fd);
    stat("/tmp/a.txt", &st);
    if (st.st_size != 0) return fail("truncate");
    printf("append/truncate: ok\n");

    // initrd stays read-only
    if (open("/hello.txt", O_WRONLY) >= 0) return fail("initrd writable");
    if (open("/newfile", O_WRONLY | O_CREAT) >= 0) return fail("create on initrd");
    if (unlink("/hello.txt") != -1) return fail("unlink on initrd");
    printf("initrd read-only: ok\n");

    // dirs
    if (mkdir("/tmp/d") < 0 || mkdir("/tmp/d") != -1) return fail("mkdir");
    fd = open("/tmp/d/inner", O_WRONLY | O_CREAT);
    if (fd < 0) return fail("create in subdir");
    write(fd, "in", 2);
    close(fd);
    if (unlink("/tmp/d") != -1) return fail("rmdir non-empty");
    if (unlink("/tmp/d/inner") != 0 || unlink("/tmp/d") != 0) return fail("remove");
    if (stat("/tmp/d", &st) == 0) return fail("still there");
    printf("mkdir/unlink: ok\n");

    // unlink an open file: still readable until closed
    fd = open("/tmp/gone", O_RDWR | O_CREAT);
    write(fd, "abc", 3);
    unlink("/tmp/gone");
    lseek(fd, 0, SEEK_SET);
    if (read(fd, buf, 3) != 3 || buf[0] != 'a') return fail("read unlinked");
    close(fd);
    printf("unlink while open: ok\n");

    // child writes, parent sees it (shared fs); shared fd offset
    fd = open("/tmp/shared", O_RDWR | O_CREAT);
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
    unlink("/tmp/shared");
    unlink("/tmp/a.txt");
    printf("fork + write: ok\n");

    // churn: create/write/delete many files, memory must not run out
    for (int i = 0; i < 300; i++) {
        char name[32] = "/tmp/f";
        name[6] = '0' + i % 10; name[7] = '0' + (i / 10) % 10; name[8] = '0' + i / 100; name[9] = 0;
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
