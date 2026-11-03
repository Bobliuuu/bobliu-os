// shared helpers for the small unix-like tools in user/. static inline only.
// note: stdout is unbuffered in our libc and main() returning calls exit directly,
// so tools write with printf/write and never use fopen'd files.
#ifndef USER_COMMON_H
#define USER_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_MAX_ 256

// small output buffer for char-at-a-time output; oflush() before exiting, printf-ing or reading a tty
static char ob_[1024];
static int on_;
static inline void oflush(void) { if (on_) write(1, ob_, on_); on_ = 0; }
static inline void oc(int c) { if (on_ == (int)sizeof(ob_)) oflush(); ob_[on_++] = (char)c; }
#define pf(...) (oflush(), printf(__VA_ARGS__))
#define DONE(rc) do { oflush(); return (rc); } while (0)
static inline void put(const char* s, int n) { for (int i = 0; i < n; i++) oc((unsigned char)s[i]); }
static inline void outf(const char* fmt, ...) {
    char t[512];
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int n = vsnprintf(t, sizeof t, fmt, ap);
    __builtin_va_end(ap);
    if (n >= (int)sizeof t) n = sizeof t - 1;
    put(t, n);
}
static inline void puts_nl(const char* s) { put(s, (int)strlen(s)); oc('\n'); }

// "prog: arg: message"
static inline void err(const char* prog, const char* arg, const char* msg) {
    oflush();
    printf("%s: %s: %s\n", prog, arg, msg);
}

// open for reading; "-" is stdin. prints an error and returns -1 on failure
static inline int open_in(const char* prog, const char* path) {
    if (path[0] == '-' && !path[1]) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) { err(prog, path, "No such file or directory"); return -1; }
    struct stat st;
    if (stat(path, &st) == 0 && st.st_isdir) {
        close(fd);
        err(prog, path, "Is a directory");
        return -1;
    }
    return fd;
}

static inline int is_dir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && st.st_isdir;
}
static inline int exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static inline void path_join(char* out, int max, const char* dir, const char* name) {
    int n = (int)strlen(dir);
    if (n >= max - 2) n = max - 2;
    memcpy(out, dir, n);
    if (n && out[n - 1] != '/') out[n++] = '/';
    int m = (int)strlen(name);
    if (n + m >= max) m = max - 1 - n;
    memcpy(out + n, name, m);
    out[n + m] = 0;
}

static inline const char* base_of(const char* p) {
    const char* s = strrchr(p, '/');
    return s ? s + 1 : p;
}

// ---- buffered line reader over a raw fd ----
typedef struct { int fd, len, pos; char buf[1024]; } Rd;
static inline void rd_init(Rd* r, int fd) { r->fd = fd; r->len = r->pos = 0; }
static inline int rd_getc(Rd* r) {
    if (r->pos >= r->len) {
        r->len = read(r->fd, r->buf, sizeof(r->buf));
        r->pos = 0;
        if (r->len <= 0) { r->len = 0; return -1; }
    }
    return (unsigned char)r->buf[r->pos++];
}
// reads a line without the newline; lines longer than max-1 are split. returns length or -1 at EOF
static inline int rd_line(Rd* r, char* out, int max) {
    int n = 0, c;
    while ((c = rd_getc(r)) >= 0) {
        if (c == '\n') { out[n] = 0; return n; }
        out[n++] = (char)c;
        if (n == max - 1) { out[n] = 0; return n; }
    }
    if (n) { out[n] = 0; return n; }
    return -1;
}

// read a whole fd into a malloc'd, NUL terminated buffer
static inline char* slurp(int fd, int* outlen) {
    int cap = 4096, len = 0;
    char* b = malloc(cap + 1);
    if (!b) { *outlen = 0; return 0; }
    for (;;) {
        if (len == cap) {
            cap *= 2;
            char* nb = realloc(b, cap + 1);
            if (!nb) break;
            b = nb;
        }
        int n = read(fd, b + len, cap - len);
        if (n <= 0) break;
        len += n;
    }
    b[len] = 0;
    *outlen = len;
    return b;
}

// split buf in place at newlines; returns the number of lines, *out is malloc'd. a trailing
// newline does not start a new (empty) line.
static inline int split_lines(char* buf, int len, char*** out) {
    int cnt = 0;
    for (int i = 0; i < len; i++) if (buf[i] == '\n') cnt++;
    if (len && buf[len - 1] != '\n') cnt++;
    char** v = malloc(sizeof(char*) * (cnt + 1));
    int k = 0, start = 0;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') { buf[i] = 0; v[k++] = buf + start; start = i + 1; }
    }
    if (start < len) v[k++] = buf + start;
    *out = v;
    return k;
}

static inline int parse_int(const char* s, int* out) {
    char* e;
    long v = strtol(s, &e, 10);
    if (e == s || *e) return -1;
    *out = (int)v;
    return 0;
}

// ---- basic regex: literal . * ^ $ [set] [^set] \c ----
static int re_icase;
static int re_ere;   /* 1: + and ? are operators after an atom */

static inline const char* re_atom_end(const char* re) {
    if (*re == '\\' && re[1]) return re + 2;
    if (*re == '[') {
        const char* p = re + 1;
        if (*p == '^') p++;
        if (*p == ']') p++;
        while (*p && *p != ']') p++;
        return *p ? p + 1 : p;
    }
    return re + 1;
}
static inline int re_ceq(int a, int b) { return re_icase ? tolower(a) == tolower(b) : a == b; }
static inline int re_atom_match(const char* re, const char* end, int c) {
    if (!c) return 0;
    if (*re == '.') return 1;
    if (*re == '\\') return re_ceq(re[1], c);
    if (*re == '[' && end[-1] == ']' && end - re >= 3) {
        const char* p = re + 1;
        int neg = 0, ok = 0;
        if (*p == '^') { neg = 1; p++; }
        for (; p < end - 1; p++) {
            if (p + 2 < end - 1 && p[1] == '-') {
                int lo = (unsigned char)p[0], hi = (unsigned char)p[2];
                if ((c >= lo && c <= hi) ||
                    (re_icase && ((tolower(c) >= lo && tolower(c) <= hi) || (toupper(c) >= lo && toupper(c) <= hi))))
                    ok = 1;
                p += 2;
            } else if (re_ceq(*p, c)) ok = 1;
        }
        return ok != neg;
    }
    return re_ceq(*re, c);
}
// match re at t; returns the end of the match or 0
static inline const char* re_here(const char* re, const char* t) {
    if (!*re) return t;
    if (re[0] == '$' && !re[1]) return *t ? 0 : t;
    const char* e = re_atom_end(re);
    if (*e == '*' || (re_ere && (*e == '+' || *e == '?'))) {
        int n = 0, min = *e == '+', max = *e == '?' ? 1 : 1 << 30;
        while (n < max && re_atom_match(re, e, (unsigned char)t[n])) n++;
        for (; n >= min; n--) {
            const char* r = re_here(e + 1, t + n);
            if (r) return r;
        }
        return 0;
    }
    if (re_atom_match(re, e, (unsigned char)*t)) return re_here(e, t + 1);
    return 0;
}
// search text starting at from; sets *ms/*me
static inline int re_search(const char* re, const char* text, const char* from, const char** ms, const char** me) {
    if (*re == '^') {
        if (from != text) return 0;
        const char* r = re_here(re + 1, from);
        if (r) { *ms = from; *me = r; return 1; }
        return 0;
    }
    for (const char* t = from;; t++) {
        const char* r = re_here(re, t);
        if (r) { *ms = t; *me = r; return 1; }
        if (!*t) return 0;
    }
}
static inline int re_test(const char* re, const char* text) {
    const char *a, *b;
    return re_search(re, text, text, &a, &b);
}

// ---- glob: * ? [set] ----
static inline int glob_match(const char* p, const char* s) {
    for (; *p; p++, s++) {
        if (*p == '*') {
            while (p[1] == '*') p++;
            if (!p[1]) return 1;
            for (; *s; s++) if (glob_match(p + 1, s)) return 1;
            return glob_match(p + 1, s);
        }
        if (!*s) return 0;
        if (*p == '?') continue;
        if (*p == '[') {
            const char* e = re_atom_end(p);
            if (e[-1] == ']') {
                int save = re_icase;
                re_icase = 0;
                int ok = re_atom_match(p, e, (unsigned char)*s);
                re_icase = save;
                if (!ok) return 0;
                p = e - 1;
                continue;
            }
        }
        if (*p == '\\' && p[1]) p++;
        if (*p != *s) return 0;
    }
    return !*s;
}

// copy a regular file; returns 0 or -1
static inline int copy_file(const char* src, const char* dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0) { close(in); return -2; }
    char buf[1024];
    int n, rc = 0;
    while ((n = read(in, buf, sizeof(buf))) > 0)
        if (write(out, buf, n) != n) { rc = -3; break; }
    close(in);
    close(out);
    return rc;
}

#endif
