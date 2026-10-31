#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int me = getpid();
    printf("parent pid=%d\n", me);

    int pid = fork();
    if (pid == 0) {
        printf("child  pid=%d (fork returned 0)\n", getpid());
        exit(7);
    }
    int status = 0;
    int got = waitpid(pid, &status, 0);
    printf("parent: reaped %d, exit status %d\n", got, WEXITSTATUS(status));

    pid = fork();
    if (pid == 0) {
        char* args[] = { "/bin/hello", 0 };
        execv("/bin/hello", args);
        printf("exec failed\n");
        exit(1);
    }
    wait(&status);
    printf("parent: exec child done, status %d\n", WEXITSTATUS(status));

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
