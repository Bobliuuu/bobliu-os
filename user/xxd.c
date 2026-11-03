// xxd [-l LEN] [-s SKIP] [-c COLS] [-p] [file]
#include "common.h"
int main(int argc, char** argv) {
    int len = -1, skip = 0, cols = 16, plain = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--")) { i++; break; }
        if (!strcmp(a, "-p") || !strcmp(a, "-ps")) { plain = 1; continue; }
        if ((a[1] == 'l' || a[1] == 's' || a[1] == 'c') && (a[2] || i + 1 < argc)) {
            int v = atoi(a[2] ? a + 2 : argv[++i]);
            if (a[1] == 'l') len = v; else if (a[1] == 's') skip = v; else cols = v;
        } else { pf("xxd: invalid option %s\n", a); return 1; }
    }
    if (cols < 1) cols = 16;
    if (cols > 64) cols = 64;
    int fd = open_in("xxd", i < argc ? argv[i] : "-");
    if (fd < 0) return 1;
    Rd r; rd_init(&r, fd);
    for (int k = 0; k < skip; k++) if (rd_getc(&r) < 0) break;
    unsigned char b[64];
    int off = skip, left = len;
    for (;;) {
        int n = 0, c;
        while (n < cols && left != 0 && (c = rd_getc(&r)) >= 0) { b[n++] = (unsigned char)c; if (left > 0) left--; }
        if (!n) break;
        if (plain) {
            for (int j = 0; j < n; j++) outf("%02x", b[j]);
            oc('\n');
        } else {
            outf("%08x: ", off);
            for (int j = 0; j < cols; j++) {
                if (j < n) outf("%02x", b[j]); else put("  ", 2);
                if (j % 2 && j + 1 < cols) oc(' ');
            }
            put("  ", 2);
            for (int j = 0; j < n; j++) oc(b[j] >= 32 && b[j] < 127 ? b[j] : '.');
            oc('\n');
        }
        off += n;
        if (n < cols) break;
    }
    DONE(0);
}
