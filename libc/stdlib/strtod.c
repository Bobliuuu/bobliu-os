// strtod: decimal (and inf/nan, hex ints) via a 64-bit mantissa scaled with long double.
// good for ordinary inputs; not correctly rounded in the last bit for every pathological string.
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

static int lc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

double strtod(const char* s, char** end) {
    const char* p = s;
    while (isspace((unsigned char)*p)) p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; } else if (*p == '+') p++;

    if (lc(p[0]) == 'i' && lc(p[1]) == 'n' && lc(p[2]) == 'f') {
        p += 3;
        if (lc(p[0]) == 'i' && lc(p[1]) == 'n' && lc(p[2]) == 'i' && lc(p[3]) == 't' && lc(p[4]) == 'y') p += 5;
        if (end) *end = (char*)p;
        return neg ? -INFINITY : INFINITY;
    }
    if (lc(p[0]) == 'n' && lc(p[1]) == 'a' && lc(p[2]) == 'n') {
        if (end) *end = (char*)(p + 3);
        return NAN;
    }
    if (p[0] == '0' && lc(p[1]) == 'x' && isxdigit((unsigned char)p[2])) {
        char* e;
        unsigned long v = strtoul(p, &e, 16);
        if (end) *end = e;
        return neg ? -(double)v : (double)v;
    }

    uint64_t m = 0;
    int ndig = 0, exp10 = 0, any = 0;
    for (; isdigit((unsigned char)*p); p++) {
        any = 1;
        if (ndig < 19) { m = m * 10 + (*p - '0'); if (m) ndig++; }
        else exp10++;
    }
    if (*p == '.') {
        p++;
        for (; isdigit((unsigned char)*p); p++) {
            any = 1;
            if (ndig < 19) { m = m * 10 + (*p - '0'); if (m) ndig++; exp10--; }
        }
    }
    if (!any) { if (end) *end = (char*)s; return 0; }
    if (lc(*p) == 'e') {
        const char* q = p + 1;
        int eneg = 0, ev = 0;
        if (*q == '-') { eneg = 1; q++; } else if (*q == '+') q++;
        if (isdigit((unsigned char)*q)) {
            for (; isdigit((unsigned char)*q); q++) if (ev < 100000) ev = ev * 10 + (*q - '0');
            exp10 += eneg ? -ev : ev;
            p = q;
        }
    }
    if (end) *end = (char*)p;

    long double v = (long double)m;
    if (m == 0) return neg ? -0.0 : 0.0;
    int e = exp10;
    long double scale = 1.0L, base = 10.0L;
    unsigned n = e < 0 ? -e : e;
    if (n > 5000) { v = e < 0 ? 0.0L : INFINITY; }
    else {
        while (n) { if (n & 1) scale *= base; base *= base; n >>= 1; }
        v = e < 0 ? v / scale : v * scale;
    }
    return neg ? -(double)v : (double)v;
}

float strtof(const char* s, char** end) { return (float)strtod(s, end); }
