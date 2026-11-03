// tr [-d] [-s] SET1 [SET2]   sets support ranges a-z and \n \t \\ escapes
#include "common.h"
static int expand(const char* s, unsigned char* out) {
    int n = 0;
    while (*s && n < 256) {
        int c = (unsigned char)*s++;
        if (c == '\\' && *s) { c = *s++; if (c == 'n') c = '\n'; else if (c == 't') c = '\t'; else if (c == 'r') c = '\r'; }
        if (*s == '-' && s[1]) {
            int e = (unsigned char)s[1];
            s += 2;
            for (int x = c; x <= e && n < 256; x++) out[n++] = (unsigned char)x;
        } else out[n++] = (unsigned char)c;
    }
    return n;
}
int main(int argc, char** argv) {
    int del = 0, sq = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'd') del = 1; else if (*p == 's') sq = 1;
            else { pf("tr: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    if (i >= argc || (!del && !sq && i + 1 >= argc)) { pf("tr: missing operand\n"); return 1; }
    unsigned char s1[256], s2[256];
    int n1 = expand(argv[i], s1), n2 = i + 1 < argc ? expand(argv[i + 1], s2) : 0;
    int map[256], inset[256];
    for (int c = 0; c < 256; c++) { map[c] = c; inset[c] = 0; }
    for (int k = 0; k < n1; k++) {
        inset[s1[k]] = 1;
        if (!del && n2) map[s1[k]] = s2[k < n2 ? k : n2 - 1];
    }
    unsigned char buf[512]; int n, last = -1;
    while ((n = read(0, buf, sizeof buf)) > 0) {
        for (int k = 0; k < n; k++) {
            int c = buf[k];
            if (del && inset[c]) continue;
            int o = del ? c : map[c];
            if (sq && inset[c] && o == last) continue;
            last = (sq && inset[c]) ? o : -1;
            oc(o);
        }
        oflush();
    }
    DONE(0);
}
