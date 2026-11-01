// bobliu user-space shell: reads a line, fork+exec's it, waits.
// supports cd/pwd/exit, "cmd | cmd | ...", and "> file" / ">> file" on the last command
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAXARGS   16
#define MAXSTAGES 6

static char* g_argv[MAXSTAGES][MAXARGS + 1];

static void exec_cmd(char** argv) {
    char path[128];
    if (strchr(argv[0], '/')) {
        strcpy(path, argv[0]);
    } else {
        strcpy(path, "/bin/");
        strcpy(path + 5, argv[0]);
    }
    execv(path, argv);
    printf("sh: %s: not found\n", argv[0]);
    exit(127);
}

static void run(int nstages, const char* redir, int append) {
    int pids[MAXSTAGES];
    int prev = -1, p[2];

    for (int i = 0; i < nstages; i++) {
        int last = i == nstages - 1;
        if (!last && pipe(p) < 0) { printf("sh: pipe failed\n"); break; }

        int pid = fork();
        if (pid < 0) { printf("sh: fork failed\n"); break; }
        if (pid == 0) {
            if (prev >= 0) { dup2(prev, 0); close(prev); }
            if (!last) { dup2(p[1], 1); close(p[0]); close(p[1]); }
            if (last && redir) {
                int fd = open(redir, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC));
                if (fd < 0) { printf("sh: cannot open %s\n", redir); exit(1); }
                dup2(fd, 1);
                close(fd);
            }
            exec_cmd(g_argv[i]);
        }
        pids[i] = pid;
        if (prev >= 0) close(prev);
        if (!last) { close(p[1]); prev = p[0]; }
    }

    int status = 0;
    for (int i = 0; i < nstages; i++) {
        if (pids[i] > 0) waitpid(pids[i], &status, 0);
    }
    if (WEXITSTATUS(status) != 0) printf("[exit %d]\n", WEXITSTATUS(status));
}

int main(void) {
    char line[128];

    printf("bobliu sh (pid %d). commands live in /bin; 'exit' leaves.\n", getpid());
    for (;;) {
        { char cwd[128]; printf("%s $ ", getcwd(cwd, sizeof(cwd)) ? cwd : "?"); }
        int n = read(0, line, sizeof(line) - 1);
        if (n <= 0) break;
        line[n] = '\0';
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';

        int stage = 0, argc = 0;
        const char* redir = 0;
        int append = 0, bad = 0;
        char* p = line;
        while (*p) {
            while (*p == ' ') *p++ = '\0';
            if (!*p) break;
            char* tok = p;
            while (*p && *p != ' ') p++;
            if (*p) *p++ = '\0';                    // terminate the word

            if (strcmp(tok, "|") == 0) {
                if (argc == 0 || stage + 1 >= MAXSTAGES) { bad = 1; break; }
                g_argv[stage][argc] = 0;
                stage++;
                argc = 0;
                continue;
            }
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
            if (argc >= MAXARGS) { bad = 1; break; }
            g_argv[stage][argc++] = tok;
        }
        g_argv[stage][argc] = 0;
        if (bad || (stage > 0 && argc == 0)) { printf("sh: syntax error\n"); continue; }
        if (stage == 0 && argc == 0) continue;

        char** argv = g_argv[0];
        if (stage == 0) {
            if (strcmp(argv[0], "exit") == 0) break;
            if (strcmp(argv[0], "cd") == 0) {
                if (chdir(argc > 1 ? argv[1] : "/") < 0) printf("cd: %s: not a directory\n", argc > 1 ? argv[1] : "/");
                continue;
            }
            if (strcmp(argv[0], "pwd") == 0) {
                char cwd[128];
                if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
                continue;
            }
        }
        run(stage + 1, redir, append);
    }
    return 0;
}
