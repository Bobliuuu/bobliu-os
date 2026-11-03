// basename path [suffix]
#include "common.h"
int main(int argc, char** argv) {
    if (argc < 2) { pf("basename: missing operand\n"); return 1; }
    char p[PATH_MAX_]; strncpy(p, argv[1], sizeof p - 1); p[sizeof p - 1] = 0;
    int n = strlen(p);
    while (n > 1 && p[n - 1] == '/') p[--n] = 0;
    const char* b = n == 1 && p[0] == '/' ? "/" : base_of(p);
    char out[PATH_MAX_]; strcpy(out, b);
    if (argc > 2) {
        int l = strlen(out), s = strlen(argv[2]);
        if (s < l && !strcmp(out + l - s, argv[2])) out[l - s] = 0;
    }
    puts_nl(out); DONE(0);
}
