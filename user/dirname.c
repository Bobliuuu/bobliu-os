#include "common.h"
int main(int argc, char** argv) {
    if (argc < 2) { pf("dirname: missing operand\n"); return 1; }
    char p[PATH_MAX_]; strncpy(p, argv[1], sizeof p - 1); p[sizeof p - 1] = 0;
    int n = strlen(p);
    while (n > 1 && p[n - 1] == '/') p[--n] = 0;
    char* s = strrchr(p, '/');
    if (!s) strcpy(p, ".");
    else { while (s > p && s[-1] == '/') s--; if (s == p) p[1] = 0; else *s = 0; }
    puts_nl(p); DONE(0);
}
