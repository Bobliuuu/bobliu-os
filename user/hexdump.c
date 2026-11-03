// hexdump [-C] [file...]: canonical hex+ascii dump (always -C style)
#include "common.h"
int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (strcmp(argv[i], "-C")) { pf("hexdump: only -C is supported\n"); return 1; }
    }
    int nf = argc - i, rc = 0, off = 0;
    unsigned char b[16], prev[16];
    int n = 0, star = 0, have = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        int fd = open_in("hexdump", nf ? argv[i + k] : "-");
        if (fd < 0) { rc = 1; continue; }
        Rd r; rd_init(&r, fd);
        int c;
        for (;;) {
            c = rd_getc(&r);
            if (c >= 0) b[n++] = (unsigned char)c;
            if (n == 16 || (c < 0 && n && k == (nf ? nf : 1) - 1)) {
                if (have && n == 16 && !memcmp(b, prev, 16)) {
                    if (!star) { puts_nl("*"); star = 1; }
                } else {
                    star = 0;
                    outf("%08x ", off);
                    for (int j = 0; j < 16; j++) {
                        if (j == 8) oc(' ');
                        if (j < n) outf(" %02x", b[j]); else put("   ", 3);
                    }
                    put("  |", 3);
                    for (int j = 0; j < n; j++) oc(b[j] >= 32 && b[j] < 127 ? b[j] : '.');
                    put("|\n", 2);
                }
                memcpy(prev, b, 16); have = (n == 16);
                off += n; n = 0;
            }
            if (c < 0) break;
        }
        if (fd) close(fd);
    }
    outf("%08x\n", off + n);
    DONE(rc);
}
