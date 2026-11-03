// cut -d C -f LIST | -c LIST [file...]   LIST: N, N-M, N-, -M, comma separated
#include "common.h"
static int lo[16], hi[16], nr;
static int parse_list(const char* s) {
    while (*s && nr < 16) {
        int a = 1, b = 100000;
        if (isdigit(*s)) a = strtol(s, (char**)&s, 10);
        if (*s == '-') { s++; if (isdigit(*s)) b = strtol(s, (char**)&s, 10); } else b = a;
        lo[nr] = a; hi[nr] = b; nr++;
        if (*s == ',') s++; else if (*s) return -1;
    }
    return nr ? 0 : -1;
}
static int in_list(int k) { for (int j = 0; j < nr; j++) if (k >= lo[j] && k <= hi[j]) return 1; return 0; }
int main(int argc, char** argv) {
    int delim = '\t', mode = 0, i = 1, sflag = 0;
    const char* list = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--")) { i++; break; }
        if (!strcmp(a, "-s")) { sflag = 1; continue; }
        if (a[1] == 'd' || a[1] == 'f' || a[1] == 'c') {
            const char* v = a[2] ? a + 2 : (i + 1 < argc ? argv[++i] : "");
            if (a[1] == 'd') delim = v[0] ? v[0] : '\t';
            else { mode = a[1]; list = v; }
        } else { pf("cut: invalid option %s\n", a); return 1; }
    }
    if (!mode || parse_list(list) < 0) { pf("cut: you must specify a list of bytes, characters, or fields\n"); return 1; }
    int nf = argc - i, rc = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        int fd = open_in("cut", nf ? argv[i + k] : "-");
        if (fd < 0) { rc = 1; continue; }
        Rd r; rd_init(&r, fd);
        char line[512];
        while (rd_line(&r, line, sizeof line) >= 0) {
            int len = strlen(line), first = 1;
            if (mode == 'c') {
                for (int j = 0; j < len; j++) if (in_list(j + 1)) oc(line[j]);
                oc('\n');
            } else {
                if (!strchr(line, delim)) { if (!sflag) puts_nl(line); continue; }
                int fld = 1, st = 0;
                for (int j = 0; j <= len; j++) {
                    if (line[j] == delim || j == len) {
                        if (in_list(fld)) { if (!first) oc(delim); put(line + st, j - st); first = 0; }
                        fld++; st = j + 1;
                    }
                }
                oc('\n');
            }
        }
        if (fd) close(fd);
    }
    DONE(rc);
}
