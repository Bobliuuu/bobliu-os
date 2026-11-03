// cmp [-s] file1 file2
#include "common.h"
int main(int argc, char** argv) {
    int silent = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--quiet")) silent = 1;
        else { pf("cmp: invalid option %s\n", argv[i]); return 2; }
    }
    if (argc - i < 2) { pf("cmp: missing operand\n"); return 2; }
    int a = open_in("cmp", argv[i]);
    if (a < 0) return 2;
    int b = open_in("cmp", argv[i + 1]);
    if (b < 0) return 2;
    Rd ra, rb; rd_init(&ra, a); rd_init(&rb, b);
    int byte = 1, line = 1;
    for (;; byte++) {
        int x = rd_getc(&ra), y = rd_getc(&rb);
        if (x < 0 && y < 0) return 0;
        if (x != y) {
            if (!silent) {
                if (x < 0 || y < 0) pf("cmp: EOF on %s after byte %d\n", x < 0 ? argv[i] : argv[i + 1], byte - 1);
                else pf("%s %s differ: byte %d, line %d\n", argv[i], argv[i + 1], byte, line);
            }
            return 1;
        }
        if (x == '\n') line++;
    }
}
