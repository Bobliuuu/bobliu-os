// xargs [-n N] [cmd [args...]]: words from stdin are appended to cmd (default echo)
#include "common.h"
#include <sys/wait.h>
#define MAXA 256
static int run(char** av) {
    char path[PATH_MAX_];
    if (strchr(av[0], '/')) { strncpy(path, av[0], sizeof path - 1); path[sizeof path - 1] = 0; }
    else path_join(path, sizeof path, "/bin", av[0]);
    oflush();
    int pid = fork();
    if (pid < 0) { err("xargs", av[0], "cannot fork"); return 1; }
    if (pid == 0) {
        execv(path, av);
        printf("xargs: %s: No such file or directory\n", av[0]);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return WEXITSTATUS(st);
}
int main(int argc, char** argv) {
    int per = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-n") && i + 1 < argc) per = atoi(argv[++i]);
        else if (!strncmp(argv[i], "-n", 2)) per = atoi(argv[i] + 2);
        else if (!strcmp(argv[i], "-r")) ;
        else { pf("xargs: invalid option %s\n", argv[i]); return 1; }
    }
    char* av[MAXA + 1];
    int base = 0;
    if (i >= argc) av[base++] = "echo";
    for (; i < argc && base < 32; i++) av[base++] = argv[i];
    int len; char* buf = slurp(0, &len);
    int n = base, rc = 0, ran = 0;
    char* p = buf;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char* w = p;
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            w = p;
            while (*p && *p != q) p++;
            if (*p) *p++ = 0;
        } else {
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) *p++ = 0;
        }
        av[n++] = w;
        if (n == MAXA || (per && n - base == per)) {
            av[n] = 0;
            if (run(av)) rc = 123;
            ran = 1; n = base;
        }
    }
    if (n > base || !ran) { av[n] = 0; if (n > base || 1) if (run(av)) rc = 123; }
    DONE(rc);
}
