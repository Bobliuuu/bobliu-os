// echo [-neE] args...
#include "common.h"
static int hexv(int c) { return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10; }
int main(int argc, char** argv) {
    int nl = 1, esc = 0, i = 1;
    for (; i < argc; i++) {
        const char* a = argv[i];
        if (a[0] != '-' || !a[1]) break;
        int ok = 1;
        for (const char* p = a + 1; *p; p++) if (*p != 'n' && *p != 'e' && *p != 'E') ok = 0;
        if (!ok) break;
        for (const char* p = a + 1; *p; p++) { if (*p == 'n') nl = 0; else if (*p == 'e') esc = 1; else esc = 0; }
    }
    for (; i < argc; i++) {
        const char* s = argv[i];
        for (; *s; s++) {
            if (esc && *s == '\\' && s[1]) {
                s++;
                switch (*s) {
                case 'n': oc('\n'); break;
                case 't': oc('\t'); break;
                case 'r': oc('\r'); break;
                case 'a': oc(7); break;
                case 'b': oc(8); break;
                case 'e': oc(27); break;
                case '\\': oc('\\'); break;
                case 'c': DONE(0);
                case '0': { int v = 0, n = 0; while (n < 3 && s[1] >= '0' && s[1] <= '7') { v = v * 8 + (*++s - '0'); n++; } oc(v); break; }
                case 'x': { int v = 0, n = 0; while (n < 2 && isxdigit((unsigned char)s[1])) { v = v * 16 + hexv(*++s); n++; } if (n) oc(v); else { oc('\\'); oc('x'); } break; }
                default: oc('\\'); oc(*s);
                }
            } else oc(*s);
        }
        if (i + 1 < argc) oc(' ');
    }
    if (nl) oc('\n');
    DONE(0);
}
