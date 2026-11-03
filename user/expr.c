// expr ARG...: | & = != < <= > >= + - * / % ( ) -- integers, string compare when not numeric
#include "common.h"
static char** tk;
static int nt, pos;
typedef struct { int isnum; int n; const char* s; } V;

static int isnum_(const char* s) {
    if (*s == '-') s++;
    if (!*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}
static V mknum(int n) { V v = {1, n, 0}; return v; }
static V mkstr(const char* s) { V v = {isnum_(s), isnum_(s) ? atoi(s) : 0, s}; return v; }
static int truthy(V v) { return v.isnum ? v.n != 0 : v.s && *v.s; }
static const char* peek(void) { return pos < nt ? tk[pos] : 0; }
static V parse_or(void);

static V primary(void) {
    const char* t = peek();
    if (!t) { pf("expr: syntax error\n"); exit(2); }
    pos++;
    if (!strcmp(t, "(")) {
        V v = parse_or();
        if (!peek() || strcmp(peek(), ")")) { pf("expr: syntax error: expecting ')'\n"); exit(2); }
        pos++;
        return v;
    }
    return mkstr(t);
}
static V mul(void) {
    V a = primary();
    for (;;) {
        const char* o = peek();
        if (!o || (strcmp(o, "*") && strcmp(o, "/") && strcmp(o, "%"))) return a;
        pos++;
        V b = primary();
        if (!a.isnum || !b.isnum) { pf("expr: non-integer argument\n"); exit(2); }
        if (o[0] != '*' && b.n == 0) { pf("expr: division by zero\n"); exit(2); }
        a = mknum(o[0] == '*' ? a.n * b.n : o[0] == '/' ? a.n / b.n : a.n % b.n);
    }
}
static V add(void) {
    V a = mul();
    for (;;) {
        const char* o = peek();
        if (!o || (strcmp(o, "+") && strcmp(o, "-"))) return a;
        pos++;
        V b = mul();
        if (!a.isnum || !b.isnum) { pf("expr: non-integer argument\n"); exit(2); }
        a = mknum(o[0] == '+' ? a.n + b.n : a.n - b.n);
    }
}
static V cmpv(void) {
    V a = add();
    for (;;) {
        const char* o = peek();
        if (!o || (strcmp(o, "=") && strcmp(o, "==") && strcmp(o, "!=") && strcmp(o, "<") && strcmp(o, "<=") && strcmp(o, ">") && strcmp(o, ">="))) return a;
        pos++;
        V b = add();
        int c;
        if (a.isnum && b.isnum) c = a.n < b.n ? -1 : a.n > b.n;
        else { char x[16], y[16]; if (a.isnum) { snprintf(x, 16, "%d", a.n); a.s = x; } if (b.isnum) { snprintf(y, 16, "%d", b.n); b.s = y; } c = strcmp(a.s, b.s); }
        int r = !strcmp(o, "=") || !strcmp(o, "==") ? c == 0 : !strcmp(o, "!=") ? c != 0 : !strcmp(o, "<") ? c < 0 : !strcmp(o, "<=") ? c <= 0 : !strcmp(o, ">") ? c > 0 : c >= 0;
        a = mknum(r);
    }
}
static V and_(void) {
    V a = cmpv();
    while (peek() && !strcmp(peek(), "&")) { pos++; V b = cmpv(); a = (truthy(a) && truthy(b)) ? a : mknum(0); }
    return a;
}
static V parse_or(void) {
    V a = and_();
    while (peek() && !strcmp(peek(), "|")) { pos++; V b = and_(); if (!truthy(a)) a = b; }
    return a;
}

int main(int argc, char** argv) {
    if (argc < 2) { pf("expr: missing operand\n"); return 2; }
    tk = argv + 1; nt = argc - 1;
    V v = parse_or();
    if (pos < nt) { pf("expr: syntax error: unexpected argument '%s'\n", tk[pos]); return 2; }
    if (v.isnum) outf("%d\n", v.n); else puts_nl(v.s);
    DONE(truthy(v) ? 0 : 1);
}
