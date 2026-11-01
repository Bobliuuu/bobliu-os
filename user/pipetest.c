#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int fail(const char* w) { printf("pipetest FAIL: %s\n", w); return 1; }

int main(void) {
    int p[2], st;
    char buf[64];

    // basic: child writes, parent reads to EOF
    if (pipe(p) < 0) return fail("pipe");
    int pid = fork();
    if (pid == 0) {
        close(p[0]);
        write(p[1], "through the pipe", 16);
        exit(0);
    }
    close(p[1]);
    memset(buf, 0, sizeof(buf));
    int n = read(p[0], buf, sizeof(buf)), total = n;
    while (n > 0 && (n = read(p[0], buf + total, sizeof(buf) - total)) > 0) total += n;
    if (strcmp(buf, "through the pipe") != 0) return fail("data");
    if (read(p[0], buf, 8) != 0) return fail("eof");
    waitpid(pid, &st, 0);
    close(p[0]);
    printf("pipe data + eof: ok\n");

    // writer blocks when full (3x the buffer), reader drains it
    pipe(p);
    pid = fork();
    if (pid == 0) {
        close(p[0]);
        static char blk[1024];
        for (int i = 0; i < 12; i++) { memset(blk, 'a' + i, sizeof(blk)); if (write(p[1], blk, sizeof(blk)) != 1024) exit(1); }
        exit(0);
    }
    close(p[1]);
    int sum = 0, bad = 0;
    static char rb[512];
    while ((n = read(p[0], rb, sizeof(rb))) > 0) {
        for (int i = 0; i < n; i++) if (rb[i] != 'a' + (sum + i) / 1024) bad++;
        sum += n;
    }
    waitpid(pid, &st, 0);
    close(p[0]);
    if (sum != 12288 || bad || WEXITSTATUS(st) != 0) return fail("big transfer");
    printf("blocking writer, 12 KiB: ok\n");

    // broken pipe: no readers left
    pipe(p);
    close(p[0]);
    if (write(p[1], "x", 1) != -1) return fail("write without reader");
    close(p[1]);
    printf("broken pipe: ok\n");

    // fds and pipe memory don't leak
    for (int i = 0; i < 100; i++) {
        if (pipe(p) < 0) return fail("pipe leak");
        close(p[0]); close(p[1]);
    }
    printf("no leak: ok\n");
    printf("pipetest done\n");
    return 0;
}
