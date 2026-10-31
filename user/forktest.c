#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static int counter = 100;   // lives in .data: shared cow after fork
static char big[3 * 4096];  // .bss spanning several pages

int main(void) {
    int me = getpid();
    printf("parent pid=%d\n", me);

    big[0] = 'p'; big[5000] = 'p';

    int pid = fork();
    if (pid == 0) {
        printf("child  pid=%d (fork returned 0)\n", getpid());
        counter += 1;
        big[0] = 'c'; big[5000] = 'c';
        exit(counter == 101 && big[0] == 'c' && big[5000] == 'c' ? 7 : 1);
    }
    int status = 0;
    int got = waitpid(pid, &status, 0);
    printf("parent: reaped %d, exit status %d\n", got, WEXITSTATUS(status));
    printf("cow isolation: %s\n", (counter == 100 && big[0] == 'p' && big[5000] == 'p') ? "ok" : "BROKEN");

    pid = fork();
    if (pid == 0) {
        char* args[] = { "/bin/hello", 0 };
        execv("/bin/hello", args);
        printf("exec failed\n");
        exit(1);
    }
    wait(&status);
    printf("parent: exec child done, status %d\n", WEXITSTATUS(status));

    // parent writes first while child still shares the page
    pid = fork();
    if (pid == 0) {
        waitpid(-1, &status, 0);          // no children: returns -1 immediately
        exit(counter == 100 ? 0 : 1);     // must still see the old value
    }
    counter = 555;
    waitpid(pid, &status, 0);
    printf("parent-writes-first: %s\n", WEXITSTATUS(status) == 0 ? "ok" : "BROKEN");

    // lots of short lived children: memory must come back
    for (int i = 0; i < 40; i++) {
        pid = fork();
        if (pid == 0) exit(i);
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status) != i) { printf("bad status at %d\n", i); return 1; }
    }
    printf("40 forks ok\n");
    return 0;
}
