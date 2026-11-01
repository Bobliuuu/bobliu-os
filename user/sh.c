// bobliu user-space shell: reads a line, fork+exec's it, waits
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAXARGS 16

static void run(char** argv, const char* redir, int append) {
    char path[128];

    if (strchr(argv[0], '/')) {
        strcpy(path, argv[0]);
    } else {
        strcpy(path, "/bin/");
        strcpy(path + 5, argv[0]);
    }

    int pid = fork();
    if (pid < 0) { printf("sh: fork failed\n"); return; }
    if (pid == 0) {
        if (redir) {
            int fd = open(redir, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC));
            if (fd < 0) { printf("sh: cannot open %s\n", redir); exit(1); }
            dup2(fd, 1);
            close(fd);
        }
        execv(path, argv);
        printf("sh: %s: not found\n", argv[0]);
        exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WEXITSTATUS(status) != 0) printf("[exit %d]\n", WEXITSTATUS(status));
}

int main(void) {
    char line[128];
    char* argv[MAXARGS + 1];

    printf("bobliu sh (pid %d). commands live in /bin; 'exit' leaves.\n", getpid());
    for (;;) {
        printf("$ ");
        int n = read(0, line, sizeof(line) - 1);
        if (n <= 0) break;
        line[n] = '\0';
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';

        int argc = 0;
        const char* redir = 0;
        int append = 0, bad = 0;
        char* p = line;
        while (*p && argc < MAXARGS) {
            while (*p == ' ') *p++ = '\0';
            if (!*p) break;
            char* tok = p;
            while (*p && *p != ' ') p++;
            if (*p) *p++ = '\0';                    // terminate the word
            if (*tok == '>') {
                append = tok[1] == '>';
                tok += append ? 2 : 1;
                if (!*tok) {                        // "> file": the name is the next word
                    while (*p == ' ') p++;
                    tok = p;
                    while (*p && *p != ' ') p++;
                    if (*p) *p++ = '\0';
                }
                if (!*tok) { bad = 1; break; }
                redir = tok;
                continue;
            }
            argv[argc++] = tok;
        }
        argv[argc] = 0;
        if (bad) { printf("sh: missing file after >\n"); continue; }
        if (argc == 0) continue;

        if (strcmp(argv[0], "exit") == 0) break;
        run(argv, redir, append);
    }
    return 0;
}
