// diff file1 file2: normal-format line diff (LCS); exit 1 if different. "-" is stdin
#include "common.h"
static char **A, **B;
static int n, m;
static unsigned short* dp;   // (n+1)*(m+1) suffix LCS lengths
#define D(i, j) dp[(i) * (m + 1) + (j)]

static void range(int s, int e) {   // 1-based inclusive
    if (e - s) outf("%d,%d", s, e); else outf("%d", s);
}

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) if (!strcmp(argv[i], "--")) { i++; break; }
    if (argc - i < 2) { pf("diff: missing operand\n"); return 2; }
    int fa = open_in("diff", argv[i]);
    if (fa < 0) return 2;
    int fb = open_in("diff", argv[i + 1]);
    if (fb < 0) return 2;
    int la, lb;
    char* ba = slurp(fa, &la);
    char* bb = (fb == fa) ? ba : slurp(fb, &lb);
    if (fb == fa) lb = la;   // both stdin: second read would be empty anyway
    if (fa) close(fa);
    if (fb) close(fb);
    n = split_lines(ba, la, &A);
    m = split_lines(bb, lb, &B);
    // trim common prefix and suffix
    int pre = 0, suf = 0;
    while (pre < n && pre < m && !strcmp(A[pre], B[pre])) pre++;
    while (suf < n - pre && suf < m - pre && !strcmp(A[n - 1 - suf], B[m - 1 - suf])) suf++;
    char **a = A + pre, **b = B + pre;
    int na = n - pre - suf, nb = m - pre - suf;
    if (!na && !nb) DONE(0);
    if ((long)(na + 1) * (nb + 1) > 2000000) { pf("diff: inputs too different/large\n"); return 2; }
    int sn = n, sm = m;
    n = na; m = nb;
    dp = malloc(sizeof(unsigned short) * (n + 1) * (m + 1));
    if (!dp) { pf("diff: out of memory\n"); return 2; }
    for (int x = n; x >= 0; x--)
        for (int y = m; y >= 0; y--) {
            if (x == n || y == m) D(x, y) = 0;
            else if (!strcmp(a[x], b[y])) D(x, y) = D(x + 1, y + 1) + 1;
            else D(x, y) = D(x + 1, y) >= D(x, y + 1) ? D(x + 1, y) : D(x, y + 1);
        }
    (void)sn; (void)sm;
    int x = 0, y = 0;
    while (x < n || y < m) {
        if (x < n && y < m && !strcmp(a[x], b[y])) { x++; y++; continue; }
        int x0 = x, y0 = y;
        while (x < n || y < m) {
            if (x < n && y < m && !strcmp(a[x], b[y])) break;
            if (y == m || (x < n && D(x + 1, y) >= D(x, y + 1))) x++; else y++;
        }
        int s1 = pre + x0 + 1, e1 = pre + x, s2 = pre + y0 + 1, e2 = pre + y;
        if (x == x0) { outf("%da", pre + x0); range(s2, e2); }
        else if (y == y0) { range(s1, e1); outf("d%d", pre + y0); }
        else { range(s1, e1); oc('c'); range(s2, e2); }
        oc('\n');
        for (int k = x0; k < x; k++) outf("< %s\n", a[k]);
        if (x > x0 && y > y0) put("---\n", 4);
        for (int k = y0; k < y; k++) outf("> %s\n", b[k]);
    }
    DONE(1);
}
