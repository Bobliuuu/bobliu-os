// vsnprintf with the usual flags: - + space 0 #, width, .precision, * for both,
// length hh h l ll z t j, conversions d i u x X o c s p %.   no floats.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct { char* p; char* end; size_t total; } out_t;

static void put(out_t* o, char c) {
    if (o->p < o->end) *o->p++ = c;
    o->total++;
}

static void pad(out_t* o, char c, int n) { while (n-- > 0) put(o, c); }

static void emit_num(out_t* o, uint64_t v, bool neg, unsigned base, bool upper,
                     bool left, bool zero, bool plus, bool space, bool alt, int width, int prec) {
    char tmp[24];
    int n = 0;
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0) { if (prec != 0) tmp[n++] = '0'; }
    while (v) { tmp[n++] = digits[v % base]; v /= base; }

    int zeros = prec > n ? prec - n : 0;
    char prefix[3]; int np = 0;
    if (neg) prefix[np++] = '-';
    else if (plus) prefix[np++] = '+';
    else if (space) prefix[np++] = ' ';
    if (alt && base == 16 && n) { prefix[np++] = '0'; prefix[np++] = upper ? 'X' : 'x'; }
    if (alt && base == 8 && zeros == 0 && (n == 0 || tmp[n - 1] != '0')) zeros = 1;

    int len = np + zeros + n;
    if (zero && !left && prec < 0 && width > len) zeros += width - len, len = width;
    if (!left) pad(o, ' ', width - len);
    for (int i = 0; i < np; i++) put(o, prefix[i]);
    pad(o, '0', zeros);
    while (n) put(o, tmp[--n]);
    if (left) pad(o, ' ', width - len);
}

int vsnprintf(char* buf, size_t size, const char* fmt, __builtin_va_list ap) {
    out_t o = { buf, buf + (size ? size - 1 : 0), 0 };

    for (; *fmt; fmt++) {
        if (*fmt != '%') { put(&o, *fmt); continue; }
        fmt++;

        bool left = false, zero = false, plus = false, space = false, alt = false;
        for (;; fmt++) {
            if (*fmt == '-') left = true;
            else if (*fmt == '0') zero = true;
            else if (*fmt == '+') plus = true;
            else if (*fmt == ' ') space = true;
            else if (*fmt == '#') alt = true;
            else break;
        }
        int width = 0, prec = -1;
        if (*fmt == '*') { width = __builtin_va_arg(ap, int); if (width < 0) { left = true; width = -width; } fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = __builtin_va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
        }
        int lng = 0;      // 0 int, 1 long, 2 long long
        int small = 0;    // 1 short, 2 char
        for (;; fmt++) {
            if (*fmt == 'l') lng++;
            else if (*fmt == 'h') small++;
            else if (*fmt == 'z' || *fmt == 't' || *fmt == 'j') lng = 1;
            else break;
        }

        switch (*fmt) {
        case '%': put(&o, '%'); break;
        case 'c': {
            char c = (char)__builtin_va_arg(ap, int);
            if (!left) pad(&o, ' ', width - 1);
            put(&o, c);
            if (left) pad(&o, ' ', width - 1);
            break;
        }
        case 's': {
            const char* s = __builtin_va_arg(ap, const char*);
            if (!s) s = "(null)";
            int n = 0;
            while (s[n] && (prec < 0 || n < prec)) n++;
            if (!left) pad(&o, ' ', width - n);
            for (int i = 0; i < n; i++) put(&o, s[i]);
            if (left) pad(&o, ' ', width - n);
            break;
        }
        case 'd': case 'i': {
            int64_t v;
            if (lng >= 2) v = __builtin_va_arg(ap, long long);
            else if (lng == 1) v = __builtin_va_arg(ap, long);
            else { v = __builtin_va_arg(ap, int); if (small == 1) v = (short)v; else if (small >= 2) v = (signed char)v; }
            bool neg = v < 0;
            emit_num(&o, neg ? (uint64_t)0 - (uint64_t)v : (uint64_t)v, neg, 10, false, left, zero, plus, space, false, width, prec);
            break;
        }
        case 'u': case 'x': case 'X': case 'o': {
            uint64_t v;
            if (lng >= 2) v = __builtin_va_arg(ap, unsigned long long);
            else if (lng == 1) v = __builtin_va_arg(ap, unsigned long);
            else { v = __builtin_va_arg(ap, unsigned); if (small == 1) v = (unsigned short)v; else if (small >= 2) v = (unsigned char)v; }
            unsigned base = *fmt == 'u' ? 10 : *fmt == 'o' ? 8 : 16;
            emit_num(&o, v, false, base, *fmt == 'X', left, zero, false, false, alt, width, prec);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)__builtin_va_arg(ap, void*);
            emit_num(&o, v, false, 16, false, left, zero, false, false, true, width, prec);
            break;
        }
        case 0: fmt--; break;      // lone % at the end
        default:
            put(&o, '%'); put(&o, *fmt);
            break;
        }
    }

    if (size) *o.p = '\0';
    return (int)o.total;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    __builtin_va_end(ap);
    return r;
}

int vsprintf(char* buf, const char* fmt, __builtin_va_list ap) {
    return vsnprintf(buf, 0x1000000, fmt, ap);   /* no real limit; -1 would wrap the end pointer */
}

int sprintf(char* buf, const char* fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(buf, 0x1000000, fmt, ap);
    __builtin_va_end(ap);
    return r;
}
