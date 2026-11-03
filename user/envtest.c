#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "child")) {          // exec'd with a custom environment
        printf("child: A=%s B=%s PATH=%s\n", getenv("A") ? getenv("A") : "(null)", getenv("B") ? getenv("B") : "(null)",
               getenv("PATH") ? getenv("PATH") : "(null)");
        return 0;
    }
    int n = 0;
    for (char** e = environ; e && *e; e++) n++;
    printf("env entries: %d, HOME=%s FOO=%s\n", n, getenv("HOME") ? getenv("HOME") : "(null)", getenv("FOO") ? getenv("FOO") : "(null)");

    setenv("A", "one", 1);
    setenv("B", "two", 1);
    setenv("A", "uno", 0);                                  // not overwritten
    unsetenv("PATH");
    printf("A=%s B=%s PATH=%s\n", getenv("A"), getenv("B"), getenv("PATH") ? getenv("PATH") : "(null)");

    int pid = fork();
    if (pid == 0) {
        char* av[] = { "/bin/envtest", "child", 0 };
        execv("/bin/envtest", av);                         // inherits our modified environ
        return 1;
    }
    int st;
    waitpid(pid, &st, 0);
    char* av2[] = { "/bin/envtest", "child", 0 };
    char* ev[] = { "A=explicit", 0 };
    pid = fork();
    if (pid == 0) { execve("/bin/envtest", av2, ev); return 1; }
    waitpid(pid, &st, 0);
    printf("envtest done\n");
    return 0;
}
