// od [-A d|o|x|n] [-t x1|o1|c|x2|o2|d1] [-b -c -x -o] [-v] [file]
#include "common.h"
int main(int argc, char** argv) {
    char fmt = 'o'; int size = 2, addr = 'o', i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--")) { i++; break; }
        if (a[1] == 'A') { addr = a[2] ? a[2] : (i + 1 < argc ? argv[++i][0] : 'o'); continue; }
        if (a[1] == 't') {
            const char* t = a[2] ? a + 2 : (i + 1 < argc ? argv[++i] : "o2");
            fmt = t[0]; size = t[1] ? atoi(t + 1) : 1; if (fmt == 'c') size = 1;
            continue;
        }
        for (const char* p = a + 1; *p; p++) {
            if (*p == 'c') { fmt = 'c'; size = 1; } else if (*p == 'b') { fmt = 'o'; size = 1; }
            else if (*p == 'x') { fmt = 'x'; size = 2; } else if (*p == 'o') { fmt = 'o'; size = 2; }
            else if (*p == 'd') { fmt = 'u'; size = 2; } else if (*p == 'v') ;
            else { pf("od: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    if (size != 1 && size != 2 && size != 4) size = 2;
    int fd = open_in("od", i < argc ? argv[i] : "-");
    if (fd < 0) return 1;
    Rd r; rd_init(&r, fd);
    unsigned char b[16];
    int off = 0;
    for (;;) {
        int n = 0, c;
        while (n < 16 && (c = rd_getc(&r)) >= 0) b[n++] = (unsigned char)c;
        if (addr != 'n') outf(addr == 'x' ? "%07x" : addr == 'd' ? "%07d" : "%07o", off);
        for (int j = 0; j < n; j += size) {
            unsigned v = 0;
            for (int k = size - 1; k >= 0; k--) v = (v << 8) | (j + k < n ? b[j + k] : 0);
            if (fmt == 'c') {
                int ch = b[j];
                if (ch == '\n') put("  \\n", 4); else if (ch == '\t') put("  \\t", 4); else if (ch == '\r') put("  \\r", 4);
                else if (ch == 0) put("  \\0", 4); else if (ch >= 32 && ch < 127) outf("   %c", ch);
                else outf(" %03o", ch);
            } else if (fmt == 'x') outf(" %0*x", size * 2, v);
            else if (fmt == 'o') outf(" %0*o", size == 1 ? 3 : size == 2 ? 6 : 11, v);
            else if (fmt == 'd') outf(" %*d", size == 1 ? 4 : size == 2 ? 6 : 11, size == 1 ? (signed char)v : size == 2 ? (short)v : (int)v);
            else outf(" %*u", size == 1 ? 3 : size == 2 ? 5 : 10, v);
        }
        if (n) oc('\n');
        off += n;
        if (n < 16) break;
    }
    if (addr != 'n') { outf(addr == 'x' ? "%07x\n" : addr == 'd' ? "%07d\n" : "%07o\n", off); }
    DONE(0);
}
