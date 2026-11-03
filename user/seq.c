// seq [first [incr]] last
#include "common.h"
int main(int argc, char** argv) {
    int v[3], n = 0;
    for (int i = 1; i < argc; i++) {
        if (n >= 3 || parse_int(argv[i], &v[n]) < 0) { pf("seq: invalid argument '%s'\n", argv[i]); return 1; }
        n++;
    }
    if (!n) { pf("seq: missing operand\n"); return 1; }
    int first = n > 1 ? v[0] : 1, inc = n == 3 ? v[1] : 1, last = v[n - 1];
    if (!inc) { pf("seq: invalid zero increment\n"); return 1; }
    char t[16];
    for (int x = first; inc > 0 ? x <= last : x >= last; x += inc) {
        snprintf(t, sizeof t, "%d\n", x);
        put(t, strlen(t));
    }
    DONE(0);
}
