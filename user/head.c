// head [-n N | -N] [file...]
#include "common.h"
int main(int argc, char** argv) {
    int n = 10, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-n")) {
            if (i + 1 >= argc || parse_int(argv[i + 1], &n) < 0) { pf("head: invalid number of lines\n"); return 1; }
            i++;
        } else if (!strncmp(argv[i], "-n", 2)) { if (parse_int(argv[i] + 2, &n) < 0) { pf("head: invalid number of lines\n"); return 1; } }
        else if (isdigit(argv[i][1])) { if (parse_int(argv[i] + 1, &n) < 0) { pf("head: invalid option\n"); return 1; } }
        else { pf("head: invalid option -- '%c'\n", argv[i][1]); return 1; }
    }
    int nf = argc - i, rc = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        const char* name = nf ? argv[i + k] : "-";
        int fd = open_in("head", name);
        if (fd < 0) { rc = 1; continue; }
        if (nf > 1) { if (k) oc('\n'); pf("==> %s <==\n", name); }
        Rd r; rd_init(&r, fd);
        int lines = 0, c;
        while (lines < n && (c = rd_getc(&r)) >= 0) { oc(c); if (c == '\n') lines++; }
        if (fd) close(fd);
    }
    DONE(rc);
}
