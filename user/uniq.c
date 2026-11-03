// uniq [-c] [-d] [-u] [file]: collapse adjacent duplicate lines
#include "common.h"
static int cflag, dflag, uflag;
static void emit(const char* l, int n) {
    if (dflag && n < 2) return;
    if (uflag && n > 1) return;
    if (cflag) { char t[16]; snprintf(t, sizeof t, "%7d ", n); put(t, (int)strlen(t)); }
    puts_nl(l);
}
int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'c') cflag = 1; else if (*p == 'd') dflag = 1; else if (*p == 'u') uflag = 1;
            else { pf("uniq: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    int fd = open_in("uniq", i < argc ? argv[i] : "-");
    if (fd < 0) return 1;
    Rd r; rd_init(&r, fd);
    char a[512], b[512], *cur = a, *nxt = b;
    int n = 0;
    if (rd_line(&r, cur, 512) < 0) DONE(0);
    n = 1;
    while (rd_line(&r, nxt, 512) >= 0) {
        if (!strcmp(cur, nxt)) n++;
        else { emit(cur, n); char* t = cur; cur = nxt; nxt = t; n = 1; }
    }
    emit(cur, n);
    DONE(0);
}
