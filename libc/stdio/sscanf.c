// scanf family over a character source: %d %i %u %x %X %o %f %e %g (with l), %s, %c, %n, %%, %*, widths.
// no %[..] sets and no %p.
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

struct src {
    int (*get)(struct src*);
    void (*unget)(struct src*, int);
    const char* s;       // string source
    FILE* f;             // file source
    int nread;
};

static int sget(struct src* r) { int c = (unsigned char)*r->s; if (!c) return EOF; r->s++; r->nread++; return c; }
static void sunget(struct src* r, int c) { (void)c; r->s--; r->nread--; }
static int fget(struct src* r) { int c = fgetc(r->f); if (c != EOF) r->nread++; return c; }
static void funget(struct src* r, int c) { ungetc(c, r->f); r->nread--; }

static int peek(struct src* r) { int c = r->get(r); if (c != EOF) r->unget(r, c); return c; }

// read a number token into buf (valid chars only); returns length
static int token(struct src* r, char* buf, int max, int base, int flt) {
    int n = 0, c, seen_digit = 0, seen_dot = 0, seen_e = 0, seen_x = 0;
    while (n < max - 1 && (c = r->get(r)) != EOF) {
        int ok = 0;
        if ((c == '+' || c == '-') && (n == 0 || (flt && (buf[n - 1] == 'e' || buf[n - 1] == 'E')))) ok = 1;
        else if (flt) {
            if (isdigit(c)) { ok = 1; seen_digit = 1; }
            else if (c == '.' && !seen_dot && !seen_e) { ok = 1; seen_dot = 1; }
            else if ((c == 'e' || c == 'E') && seen_digit && !seen_e) { ok = 1; seen_e = 1; }
        } else {
            if ((c == 'x' || c == 'X') && (base == 16 || base == 0) && !seen_x && seen_digit && n >= 1 && buf[n - 1] == '0') { ok = 1; seen_x = 1; base = 16; }
            else if (base == 16 || seen_x) ok = isxdigit(c);
            else if (base == 8) ok = c >= '0' && c <= '7';
            else ok = isdigit(c);
            if (ok && c != 'x' && c != 'X') seen_digit = 1;
        }
        if (!ok) { r->unget(r, c); break; }
        buf[n++] = (char)c;
    }
    buf[n] = 0;
    return n;
}

static int vscan(struct src* r, const char* fmt, va_list ap) {
    int count = 0;
    for (; *fmt; fmt++) {
        if (isspace((unsigned char)*fmt)) { int c; while ((c = r->get(r)) != EOF && isspace(c)) {} if (c != EOF) r->unget(r, c); continue; }
        if (*fmt != '%') { int c = r->get(r); if (c != *fmt) { if (c != EOF) r->unget(r, c); return count ? count : (c == EOF ? EOF : 0); } continue; }
        fmt++;
        int skip = 0, width = 0, lng = 0;
        if (*fmt == '*') { skip = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'L') { if (*fmt == 'l' || *fmt == 'L') lng = 1; fmt++; }
        char cv = *fmt;
        if (cv == '%') { int c = r->get(r); if (c != '%') return count; continue; }
        if (cv == 'n') { if (!skip) *va_arg(ap, int*) = r->nread; continue; }
        if (cv != 'c') { int c; while ((c = r->get(r)) != EOF && isspace(c)) {} if (c != EOF) r->unget(r, c); }
        if (peek(r) == EOF) return count ? count : EOF;

        if (cv == 'c') {
            int c = r->get(r);
            if (!skip) *va_arg(ap, char*) = (char)c;
        } else if (cv == 's') {
            char* d = skip ? 0 : va_arg(ap, char*);
            int n = 0, c;
            while ((c = r->get(r)) != EOF) {
                if (isspace(c) || (width && n >= width)) { r->unget(r, c); break; }
                if (d) d[n] = (char)c;
                n++;
            }
            if (d) d[n] = 0;
        } else if (cv == 'd' || cv == 'i' || cv == 'u' || cv == 'x' || cv == 'X' || cv == 'o') {
            char buf[40];
            int base = (cv == 'x' || cv == 'X') ? 16 : cv == 'o' ? 8 : cv == 'i' ? 0 : 10;
            int n = token(r, buf, sizeof buf, base, 0);
            if (n == 0 || (n == 1 && (buf[0] == '+' || buf[0] == '-'))) return count;
            unsigned long v = strtoul(buf, 0, base);
            if (!skip) { if (lng) *va_arg(ap, unsigned long*) = v; else *va_arg(ap, unsigned*) = (unsigned)v; }
        } else if (cv == 'f' || cv == 'e' || cv == 'g' || cv == 'E' || cv == 'G') {
            char buf[64];
            int n = token(r, buf, sizeof buf, 10, 1);
            if (n == 0) return count;
            double v = strtod(buf, 0);
            if (!skip) { if (lng) *va_arg(ap, double*) = v; else *va_arg(ap, float*) = (float)v; }
        } else return count;
        if (!skip) count++;
    }
    return count;
}

int sscanf(const char* s, const char* fmt, ...) {
    struct src r = { sget, sunget, s, 0, 0 };
    va_list ap;
    va_start(ap, fmt);
    int n = vscan(&r, fmt, ap);
    va_end(ap);
    return n;
}

int fscanf(FILE* f, const char* fmt, ...) {
    struct src r = { fget, funget, 0, f, 0 };
    va_list ap;
    va_start(ap, fmt);
    int n = vscan(&r, fmt, ap);
    va_end(ap);
    return n;
}
