// test EXPR / [ EXPR ]: -e -f -d -s -r -w -x -z -n  = == != -eq -ne -lt -gt -le -ge  ! -a -o ( )
#include "common.h"
static char** a;
static int n, p;

static int is_binop(const char* s) {
    static const char* ops[] = {"=", "==", "!=", "-eq", "-ne", "-lt", "-gt", "-le", "-ge", "<", ">"};
    for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++) if (!strcmp(s, ops[i])) return 1;
    return 0;
}
static int expr_or(void);

static int primary(void) {
    if (p >= n) return 0;
    const char* t = a[p];
    if (!strcmp(t, "(")) {
        p++;
        int v = expr_or();
        if (p < n && !strcmp(a[p], ")")) p++;
        return v;
    }
    if (!strcmp(t, "!")) { p++; return !primary(); }
    // binary: A op B
    if (p + 2 < n + 0 && is_binop(a[p + 1])) {
        const char *x = a[p], *o = a[p + 1], *y = a[p + 2];
        p += 3;
        if (!strcmp(o, "=") || !strcmp(o, "==")) return !strcmp(x, y);
        if (!strcmp(o, "!=")) return strcmp(x, y) != 0;
        if (!strcmp(o, "<")) return strcmp(x, y) < 0;
        if (!strcmp(o, ">")) return strcmp(x, y) > 0;
        int u = atoi(x), v = atoi(y);
        return !strcmp(o, "-eq") ? u == v : !strcmp(o, "-ne") ? u != v : !strcmp(o, "-lt") ? u < v : !strcmp(o, "-gt") ? u > v : !strcmp(o, "-le") ? u <= v : u >= v;
    }
    // unary: -x ARG
    if (t[0] == '-' && t[1] && !t[2] && p + 1 < n && strchr("efdsrwxzn", t[1])) {
        const char* arg = a[p + 1];
        p += 2;
        struct stat st;
        switch (t[1]) {
        case 'e': case 'r': case 'w': case 'x': return stat(arg, &st) == 0;
        case 'f': return stat(arg, &st) == 0 && !st.st_isdir;
        case 'd': return stat(arg, &st) == 0 && st.st_isdir;
        case 's': return stat(arg, &st) == 0 && st.st_size > 0;
        case 'z': return !*arg;
        default: return *arg != 0;
        }
    }
    p++;
    return *t != 0;
}
static int expr_and(void) {
    int v = primary();
    while (p < n && !strcmp(a[p], "-a")) { p++; int w = primary(); v = v && w; }
    return v;
}
static int expr_or(void) {
    int v = expr_and();
    while (p < n && !strcmp(a[p], "-o")) { p++; int w = expr_and(); v = v || w; }
    return v;
}

int main(int argc, char** argv) {
    a = argv + 1; n = argc - 1;
    if (!strcmp(base_of(argv[0]), "[")) {
        if (n < 1 || strcmp(a[n - 1], "]")) { pf("[: missing ']'\n"); return 2; }
        n--;
    }
    if (!n) return 1;
    int v = expr_or();
    if (p < n) { pf("test: extra argument '%s'\n", a[p]); return 2; }
    return v ? 0 : 1;
}
