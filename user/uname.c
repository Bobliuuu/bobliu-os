// uname [-asnrvm]
#include "common.h"
int main(int argc, char** argv) {
    int all = 0, s = 0, n = 0, r = 0, v = 0, m = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') { pf("uname: extra operand '%s'\n", argv[i]); return 1; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'a') all = 1; else if (*p == 's') s = 1; else if (*p == 'n') n = 1;
            else if (*p == 'r') r = 1; else if (*p == 'v') v = 1; else if (*p == 'm') m = 1;
            else { pf("uname: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    if (!(all || s || n || r || v || m)) s = 1;
    if (all) s = n = r = v = m = 1;
    int first = 1;
    #define F(c, t) if (c) { if (!first) oc(' '); put(t, strlen(t)); first = 0; }
    F(s, "bobliu") F(n, "bobliu") F(r, "0.1") F(v, "#1 bobliu") F(m, "i686")
    oc('\n'); DONE(0);
}
