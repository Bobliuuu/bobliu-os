// base64 [-d] [-w COLS] [file]
#include "common.h"
static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int col, wrap = 76;
static void emit(int c) {
    if (wrap && col == wrap) { oc('\n'); col = 0; }
    oc(c); col++;
}
static int val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
int main(int argc, char** argv) {
    int dec = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--decode")) dec = 1;
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) wrap = atoi(argv[++i]);
        else if (!strncmp(argv[i], "-w", 2)) wrap = atoi(argv[i] + 2);
        else { pf("base64: invalid option %s\n", argv[i]); return 1; }
    }
    int fd = open_in("base64", i < argc ? argv[i] : "-");
    if (fd < 0) return 1;
    Rd r; rd_init(&r, fd);
    int c;
    if (dec) {
        int acc = 0, bits = 0;
        while ((c = rd_getc(&r)) >= 0) {
            if (c == '=') break;
            int v = val(c);
            if (v < 0) continue;
            acc = (acc << 6) | v; bits += 6;
            if (bits >= 8) { bits -= 8; oc((acc >> bits) & 255); }
        }
    } else {
        unsigned char b[3]; int n = 0;
        for (;;) {
            c = rd_getc(&r);
            if (c >= 0) b[n++] = (unsigned char)c;
            if (n == 3 || (c < 0 && n)) {
                emit(T[b[0] >> 2]);
                emit(T[((b[0] & 3) << 4) | (n > 1 ? b[1] >> 4 : 0)]);
                emit(n > 1 ? T[((b[1] & 15) << 2) | (n > 2 ? b[2] >> 6 : 0)] : '=');
                emit(n > 2 ? T[b[2] & 63] : '=');
                n = 0;
            }
            if (c < 0) break;
        }
        if (col) oc('\n');
    }
    DONE(0);
}
