// tail [-n N | -N | -n +N] [file...]  (reads the whole input)
#include "common.h"
int main(int argc, char** argv) {
    int n = 10, from = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char* v = 0;
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-n")) { if (i + 1 >= argc) { pf("tail: option requires an argument\n"); return 1; } v = argv[++i]; }
        else if (!strncmp(argv[i], "-n", 2)) v = argv[i] + 2;
        else if (isdigit(argv[i][1])) v = argv[i] + 1;
        else { pf("tail: invalid option -- '%c'\n", argv[i][1]); return 1; }
        if (*v == '+') { from = 1; v++; }
        if (parse_int(v, &n) < 0) { pf("tail: invalid number of lines\n"); return 1; }
    }
    int nf = argc - i, rc = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        const char* name = nf ? argv[i + k] : "-";
        int fd = open_in("tail", name);
        if (fd < 0) { rc = 1; continue; }
        int len; char* buf = slurp(fd, &len); char** v;
        if (fd) close(fd);
        int cnt = split_lines(buf, len, &v);
        if (nf > 1) { if (k) oc('\n'); pf("==> %s <==\n", name); }
        int s = from ? (n > 0 ? n - 1 : 0) : (cnt > n ? cnt - n : 0);
        for (int j = s; j < cnt; j++) puts_nl(v[j]);
        free(v); free(buf);
    }
    DONE(rc);
}
