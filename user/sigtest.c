#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static volatile int got, nested_calls, last_sig;

static void h1(int sig) { got++; last_sig = sig; }
static void h_nested(int sig) {
    nested_calls++;
    if (nested_calls == 1) kill(getpid(), sig);   // blocked while we are inside: runs after we return
    if (nested_calls == 1 && got == 0) got = 1;   // would be 2 if it recursed
}

static int fail(const char* w) { printf("sigtest FAIL: %s\n", w); return 1; }

int main(int argc, char** argv) {
    (void)argv;
    if (argc > 1) {                 // re-exec'd child: a handler set before exec must be gone
        kill(getpid(), SIGUSR1);
        exit(0);
    }
    int st, p[2];
    char c;

    // handler runs before kill() returns, registers survive
    signal(SIGUSR1, h1);
    int a = 11, b = 22, d = 33;
    kill(getpid(), SIGUSR1);
    if (got != 1 || last_sig != SIGUSR1 || a != 11 || b != 22 || d != 33) return fail("handler");
    printf("handler + sigreturn: ok\n");

    // same signal is blocked while its handler runs, delivered afterwards
    got = 0;
    signal(SIGUSR2, h_nested);
    kill(getpid(), SIGUSR2);
    if (nested_calls != 2) return fail("blocked while in handler");
    printf("mask during handler: ok\n");

    if (kill(9999, SIGTERM) != -1) return fail("kill bad pid");
    if (signal(SIGKILL, h1) != SIG_ERR) return fail("catch sigkill");

    // SIGKILL stops a child that is spinning in user mode (delivered after a timer irq)
    int pid = fork();
    if (pid == 0) { for (;;); }
    for (volatile int i = 0; i < 2000000; i++);
    kill(pid, SIGKILL);
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 128 + SIGKILL) return fail("sigkill spinner");
    printf("sigkill on a running child: ok\n");

    // default action wakes a child sleeping in a pipe read
    pipe(p);
    pid = fork();
    if (pid == 0) { close(p[1]); read(p[0], &c, 1); exit(0); }   // never returns normally
    for (volatile int i = 0; i < 2000000; i++);
    kill(pid, SIGTERM);
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 128 + SIGTERM) return fail("sigterm sleeper");
    close(p[0]); close(p[1]);
    printf("sigterm interrupts a sleeping child: ok\n");

    // ignored signals do nothing
    int q[2];
    pipe(p); pipe(q);
    pid = fork();
    if (pid == 0) {
        signal(SIGUSR1, SIG_IGN);
        write(q[1], "r", 1);
        read(p[0], &c, 1);                       // parent sends "g" after the kill
        exit(5);
    }
    read(q[0], &c, 1);
    kill(pid, SIGUSR1);
    write(p[1], "g", 1);
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 5) return fail("ignored signal");
    printf("SIG_IGN: ok\n");

    // handler + blocked read: read gives up (-1), handler has run
    pipe(p); pipe(q);
    pid = fork();
    if (pid == 0) {
        got = 0;
        signal(SIGUSR1, h1);
        write(q[1], "r", 1);                     // handler is installed
        int r = read(p[0], &c, 1);
        exit(r == -1 && got == 1 ? 3 : 4);
    }
    read(q[0], &c, 1);
    kill(pid, SIGUSR1);
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 3) return fail("interrupted read");
    printf("handler interrupts a blocking read: ok\n");

    // exec resets caught handlers to default
    pid = fork();
    if (pid == 0) {
        signal(SIGUSR1, h1);
        char* args[] = { "/bin/sigtest", "child", 0 };
        execv("/bin/sigtest", args);
        exit(99);
    }
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 128 + SIGUSR1) return fail("exec reset");
    printf("exec resets handlers: ok\n");
    printf("sigtest done\n");
    return 0;
}
