#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

static int fail(const char* w) { printf("timetest FAIL: %s\n", w); return 1; }

static int ms_since_boot(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void noop(int s) { (void)s; }

int main(void) {
    time_t t = time(0);
    struct tm tm;
    gmtime_r(&t, &tm);
    if (tm.tm_year + 1900 < 2024) return fail("wall clock before 2024");
    printf("wall clock: %d-%d-%d %d:%d:%d utc\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    int a = ms_since_boot();
    sleep(1);
    int b = ms_since_boot();
    if (b - a < 990 || b - a > 1300) { printf("slept %d ms\n", b - a); return fail("sleep(1)"); }
    printf("sleep(1) took %d ms: ok\n", b - a);

    a = ms_since_boot();
    usleep(250000);
    b = ms_since_boot();
    if (b - a < 240 || b - a > 400) { printf("slept %d ms\n", b - a); return fail("usleep"); }
    printf("usleep(250000) took %d ms: ok\n", b - a);

    // time() agrees with the monotonic clock over a sleep
    time_t t1 = time(0);
    sleep(2);
    if (time(0) - t1 < 2 || time(0) - t1 > 3) return fail("time() across sleep");
    printf("time() advances: ok\n");

    // a signal cuts a sleep short
    signal(SIGUSR1, noop);
    int pid = fork();
    if (pid == 0) {
        int start = ms_since_boot();
        int r = nanosleep(&(struct timespec){ 5, 0 }, 0);
        exit(r == -1 && ms_since_boot() - start < 3000 ? 0 : 1);
    }
    sleep(1);
    kill(pid, SIGUSR1);
    int st;
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 0) return fail("signal vs nanosleep");
    printf("signal interrupts sleep: ok\n");

    // several sleepers at once all wake on time
    int t0 = ms_since_boot();
    for (int i = 1; i <= 3; i++) {
        if (fork() == 0) { usleep(300000u * i); exit(0); }
    }
    while (wait(&st) > 0);
    int took = ms_since_boot() - t0;
    if (took < 880 || took > 1300) { printf("took %d ms\n", took); return fail("parallel sleepers"); }
    printf("3 parallel sleepers: ok\n");
    printf("timetest done\n");
    return 0;
}
