// kill [-SIG|-s SIG] pid...   SIG: number or HUP INT KILL USR1 USR2 SEGV TERM
#include "common.h"
#include <signal.h>
int main(int argc, char** argv) {
    int sig = SIGTERM, i = 1;
    static const struct { const char* n; int v; } T[] = {{"HUP",1},{"INT",2},{"KILL",9},{"USR1",10},{"SEGV",11},{"USR2",12},{"TERM",15}};
    if (i < argc && argv[i][0] == '-' && argv[i][1]) {
        const char* s = argv[i] + 1;
        if (!strcmp(argv[i], "-s") && i + 1 < argc) s = argv[++i];
        if (!strncmp(s, "SIG", 3)) s += 3;
        int v = -1;
        if (isdigit(*s)) parse_int(s, &v);
        else for (unsigned k = 0; k < sizeof T / sizeof *T; k++) if (!strcmp(T[k].n, s)) v = T[k].v;
        if (v < 0) { pf("kill: invalid signal '%s'\n", s); return 1; }
        sig = v; i++;
    }
    if (i >= argc) { pf("usage: kill [-SIG] pid...\n"); return 1; }
    int rc = 0;
    for (; i < argc; i++) {
        int pid;
        if (parse_int(argv[i], &pid) < 0) { err("kill", argv[i], "arguments must be process IDs"); rc = 1; }
        else if (kill(pid, sig) < 0) { err("kill", argv[i], "No such process"); rc = 1; }
    }
    return rc;
}
