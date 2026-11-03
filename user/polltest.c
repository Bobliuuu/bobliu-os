#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>

static long ms(void) { struct timespec t; clock_gettime(1, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static int bad;
#define CHECK(c, what) do { if (!(c)) { printf("FAIL: %s\n", what); bad = 1; } else printf("ok: %s\n", what); } while (0)

int main(void) {
    int fd[2];
    char b[16];
    pipe(fd);
    fcntl(fd[0], F_SETFL, O_NONBLOCK);
    CHECK(fcntl(fd[0], F_GETFL) & O_NONBLOCK, "F_GETFL shows O_NONBLOCK");
    CHECK(read(fd[0], b, 8) == -1 && errno == EAGAIN, "empty nonblocking read -> EAGAIN");

    struct pollfd p = { fd[0], POLLIN, 0 };
    long t0 = ms();
    int r = poll(&p, 1, 150);
    long dt = ms() - t0;
    CHECK(r == 0 && dt >= 140 && dt < 400, "poll timeout ~150ms");
    printf("   (took %ld ms)\n", dt);

    int pid = fork();
    if (pid == 0) { usleep(200000); write(fd[1], "hi", 2); _exit(0); }
    p.revents = 0;
    t0 = ms();
    r = poll(&p, 1, -1);
    dt = ms() - t0;
    CHECK(r == 1 && (p.revents & POLLIN) && dt >= 150 && dt < 500, "poll wakes when child writes");
    printf("   (took %ld ms)\n", dt);
    CHECK(read(fd[0], b, 8) == 2, "read 2 bytes");
    int st; waitpid(pid, &st, 0);

    // writer end full
    fcntl(fd[1], F_SETFL, O_NONBLOCK);
    static char blk[1024];
    int total = 0, w;
    while ((w = write(fd[1], blk, sizeof blk)) > 0) total += w;
    CHECK(total == 4096 && errno == EAGAIN, "full pipe -> EAGAIN after 4096");
    struct pollfd pw = { fd[1], POLLOUT, 0 };
    CHECK(poll(&pw, 1, 0) == 0, "full pipe not writable");
    while (read(fd[0], b, 16) > 0);
    CHECK(poll(&pw, 1, 0) == 1 && (pw.revents & POLLOUT), "drained pipe writable");

    close(fd[1]);
    p.revents = 0;
    CHECK(poll(&p, 1, 0) == 1 && (p.revents & POLLHUP), "closed writer -> POLLHUP");
    CHECK(read(fd[0], b, 8) == 0, "EOF reads 0");

    // select
    pipe(fd);
    fd_set rs; FD_ZERO(&rs); FD_SET(fd[0], &rs);
    struct timeval tv = { 0, 100000 };
    t0 = ms();
    CHECK(select(fd[0] + 1, &rs, 0, 0, &tv) == 0, "select timeout");
    write(fd[1], "x", 1);
    FD_ZERO(&rs); FD_SET(fd[0], &rs);
    CHECK(select(fd[0] + 1, &rs, 0, 0, &tv) == 1 && FD_ISSET(fd[0], &rs), "select readable");

    // stdin
    struct pollfd si = { 0, POLLIN, 0 };
    CHECK(poll(&si, 1, 50) == 0, "stdin idle");
    int fl = fcntl(0, F_GETFL);
    fcntl(0, F_SETFL, fl | O_NONBLOCK);
    CHECK(read(0, b, 8) == -1 && errno == EAGAIN, "stdin nonblocking -> EAGAIN");
    fcntl(0, F_SETFL, fl);
    printf("type a line now (waiting up to 20s)...\n");
    r = poll(&si, 1, 20000);
    CHECK(r == 1 && (si.revents & POLLIN), "stdin readable after line typed");
    r = read(0, b, 15);
    b[r > 0 ? r : 0] = 0;
    printf("got '%s'\n", b);
    printf(bad ? "polltest FAILED\n" : "polltest done\n");
    return bad;
}
