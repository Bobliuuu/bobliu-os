// sort [-r] [-n] [-u] [file...]
#include "common.h"
static int rev, num;
static int key(const char* s) { while (*s == ' ' || *s == '\t') s++; return atoi(s); }
static int cmp(const void* a, const void* b) {
    const char *x = *(char* const*)a, *y = *(char* const*)b;
    int r;
    if (num) { int p = key(x), q = key(y); r = p < q ? -1 : p > q; if (!r) r = strcmp(x, y); }
    else r = strcmp(x, y);
    return rev ? -r : r;
}
int main(int argc, char** argv) {
    int uniq = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'r') rev = 1; else if (*p == 'n') num = 1; else if (*p == 'u') uniq = 1;
            else { pf("sort: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    int nf = argc - i, rc = 0, tot = 0, cap = 256;
    char** all = malloc(sizeof(char*) * cap);
    for (int k = 0; k < (nf ? nf : 1); k++) {
        int fd = open_in("sort", nf ? argv[i + k] : "-");
        if (fd < 0) { rc = 2; continue; }
        int len; char* buf = slurp(fd, &len); char** v;
        if (fd) close(fd);
        int cnt = split_lines(buf, len, &v);
        for (int j = 0; j < cnt; j++) {
            if (tot == cap) { cap *= 2; all = realloc(all, sizeof(char*) * cap); }
            all[tot++] = v[j];
        }
        free(v);
    }
    qsort(all, tot, sizeof(char*), cmp);
    for (int j = 0; j < tot; j++) {
        if (uniq && j && cmp(&all[j - 1], &all[j]) == 0) continue;
        puts_nl(all[j]);
    }
    DONE(rc);
}
