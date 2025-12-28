#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// To enable %f
// CPPFLAGS += -DPRINTF_ENABLE_FLOAT=1

// ---------- low-level output ----------
static bool print_buf(const char* data, size_t length) {
    const unsigned char* bytes = (const unsigned char*)data;
    for (size_t i = 0; i < length; i++) {
        if (putchar(bytes[i]) == EOF) return false;
    }
    return true;
}

static bool print_ch(char c) { return print_buf(&c, 1); }

// ---------- formatting helpers ----------
static size_t u64_to_base(char* out_rev, uint64_t v, unsigned base, bool upper) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t n = 0;
    if (v == 0) {
        out_rev[n++] = '0';
        return n;
    }
    while (v) {
        out_rev[n++] = digits[v % base];
        v /= base;
    }
    return n; // reversed
}

static bool pad(int count, char padch) {
    for (int i = 0; i < count; i++) {
        if (!print_ch(padch)) return false;
    }
    return true;
}

struct fmt {
    bool left;     // '-'
    bool zero;     // '0'
    bool plus;     // '+'
    bool space;    // ' '
    bool alt;      // '#'
    int  width;    // field width
    int  prec;     // precision; -1 = not specified
    enum { LEN_NONE, LEN_L, LEN_LL } len;
    bool upper;
};

static const char* parse_int(const char* s, int* out) {
    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    *out = v;
    return s;
}

static const char* parse_format(const char* s, struct fmt* f, va_list* ap) {
    // flags
    for (;;) {
        if (*s == '-') { f->left = true; s++; continue; }
        if (*s == '0') { f->zero = true; s++; continue; }
        if (*s == '+') { f->plus = true; s++; continue; }
        if (*s == ' ') { f->space = true; s++; continue; }
        if (*s == '#') { f->alt = true; s++; continue; }
        break;
    }

    // width
    f->width = 0;
    if (*s == '*') {
        int w = va_arg(*ap, int);
        if (w < 0) { f->left = true; w = -w; }
        f->width = w;
        s++;
    } else if (*s >= '0' && *s <= '9') {
        s = parse_int(s, &f->width);
    }

    // precision
    f->prec = -1;
    if (*s == '.') {
        s++;
        if (*s == '*') {
            int p = va_arg(*ap, int);
            f->prec = (p < 0) ? -1 : p;
            s++;
        } else {
            int p = 0;
            s = parse_int(s, &p);
            f->prec = p;
        }
    }

    // length
    f->len = LEN_NONE;
    if (*s == 'l') {
        s++;
        if (*s == 'l') { f->len = LEN_LL; s++; }
        else f->len = LEN_L;
    }

    return s;
}

static bool print_signed(struct fmt* f, int64_t val) {
    bool neg = (val < 0);
    uint64_t mag = neg ? (uint64_t)(-(val + 1)) + 1ULL : (uint64_t)val;

    char tmp[32];
    size_t nrev = u64_to_base(tmp, mag, 10, false);

    // precision: minimum digits
    size_t ndigits = nrev;
    size_t need_digits = (f->prec >= 0 && (size_t)f->prec > ndigits) ? (size_t)f->prec : ndigits;

    char signch = 0;
    if (neg) signch = '-';
    else if (f->plus) signch = '+';
    else if (f->space) signch = ' ';

    // width calculation
    size_t total = need_digits + (signch ? 1 : 0);
    int padcount = (f->width > (int)total) ? (f->width - (int)total) : 0;

    // if precision specified, '0' flag is ignored
    char padch = (!f->left && f->zero && f->prec < 0) ? '0' : ' ';

    if (!f->left && padch == ' ') if (!pad(padcount, ' ')) return false;

    if (signch) {
        if (!print_ch(signch)) return false;
    }

    if (!f->left && padch == '0') if (!pad(padcount, '0')) return false;

    // leading zeros for precision
    if (need_digits > ndigits) {
        if (!pad((int)(need_digits - ndigits), '0')) return false;
    }

    // print digits (reverse)
    while (nrev--) {
        if (!print_ch(tmp[nrev])) return false;
    }

    if (f->left) if (!pad(padcount, ' ')) return false;

    return true;
}

static bool print_unsigned(struct fmt* f, uint64_t val, unsigned base, bool alt_prefix) {
    char tmp[64];
    size_t nrev = u64_to_base(tmp, val, base, f->upper);

    // precision handling: special case precision 0 and value 0 -> print nothing
    if (f->prec == 0 && val == 0) nrev = 0;

    size_t ndigits = nrev;
    size_t need_digits = (f->prec >= 0 && (size_t)f->prec > ndigits) ? (size_t)f->prec : ndigits;

    const char* prefix = "";
    size_t prefix_len = 0;

    if (alt_prefix && f->alt && val != 0) {
        if (base == 16) { prefix = f->upper ? "0X" : "0x"; prefix_len = 2; }
    }

    size_t total = prefix_len + need_digits;
    int padcount = (f->width > (int)total) ? (f->width - (int)total) : 0;

    char padch = (!f->left && f->zero && f->prec < 0) ? '0' : ' ';

    if (!f->left && padch == ' ') if (!pad(padcount, ' ')) return false;

    // If zero-padding and there's a prefix, print prefix first (like real printf)
    if (prefix_len && padch == '0') {
        if (!print_buf(prefix, prefix_len)) return false;
        prefix_len = 0; // consumed
    }

    if (!f->left && padch == '0') if (!pad(padcount, '0')) return false;

    if (prefix_len) {
        if (!print_buf(prefix, prefix_len)) return false;
    }

    if (need_digits > ndigits) {
        if (!pad((int)(need_digits - ndigits), '0')) return false;
    }

    while (nrev--) {
        if (!print_ch(tmp[nrev])) return false;
    }

    if (f->left) if (!pad(padcount, ' ')) return false;

    return true;
}

static bool print_string(struct fmt* f, const char* s) {
    if (!s) s = "(null)";
    size_t len = strlen(s);
    if (f->prec >= 0 && (size_t)f->prec < len) len = (size_t)f->prec;

    int padcount = (f->width > (int)len) ? (f->width - (int)len) : 0;

    if (!f->left) if (!pad(padcount, ' ')) return false;
    if (!print_buf(s, len)) return false;
    if (f->left) if (!pad(padcount, ' ')) return false;
    return true;
}

#ifndef PRINTF_ENABLE_FLOAT
#define PRINTF_ENABLE_FLOAT 0
#endif

#if PRINTF_ENABLE_FLOAT
static bool print_float(struct fmt* f, double x) {
    // Fixed-point only: %f / %.Nf
    // WARNING: requires sane FPU/SSE state; keep disabled until you manage FPU context.
    int prec = (f->prec >= 0) ? f->prec : 6;
    if (prec > 9) prec = 9;

    // sign
    bool neg = (x < 0.0);
    if (neg) x = -x;

    // integer part
    uint64_t ip = (uint64_t)x;
    double frac = x - (double)ip;

    // scale fractional
    uint64_t scale = 1;
    for (int i = 0; i < prec; i++) scale *= 10;

    uint64_t fp = (uint64_t)(frac * (double)scale + 0.5); // rounded
    if (fp >= scale) { fp = 0; ip += 1; }

    // Build into a temporary string so width/pad works simply
    char buf[128];
    size_t pos = 0;

    if (neg) buf[pos++] = '-';
    else if (f->plus) buf[pos++] = '+';
    else if (f->space) buf[pos++] = ' ';

    // integer to string (normal order)
    char tmp[32];
    size_t nrev = u64_to_base(tmp, ip, 10, false);
    while (nrev--) buf[pos++] = tmp[nrev];

    buf[pos++] = '.';

    // fractional with leading zeros to prec
    for (int i = prec - 1; i >= 0; i--) {
        buf[pos + (size_t)i] = (char)('0' + (fp % 10));
        fp /= 10;
    }
    pos += (size_t)prec;

    buf[pos] = '\0';

    // print as string with width
    struct fmt sf = *f;
    sf.prec = -1;
    return print_string(&sf, buf);
}
#endif

int printf(const char* restrict format, ...) {
    va_list ap;
    va_start(ap, format);

    int written = 0;

    while (*format) {
        // fast path: copy plain text until '%'
        if (*format != '%') {
            const char* start = format;
            while (*format && *format != '%') format++;
            size_t len = (size_t)(format - start);
            if (!print_buf(start, len)) { va_end(ap); return -1; }
            written += (int)len;
            continue;
        }

        // handle %%
        if (format[0] == '%' && format[1] == '%') {
            if (!print_ch('%')) { va_end(ap); return -1; }
            format += 2;
            written += 1;
            continue;
        }

        // parse format
        format++; // skip '%'
        struct fmt f = {0};
        f.width = 0;
        f.prec = -1;
        f.len = LEN_NONE;
        f.upper = false;

        format = parse_format(format, &f, &ap);
        char spec = *format ? *format++ : '\0';

        switch (spec) {
            case 'c': {
                char c = (char)va_arg(ap, int);
                // treat as 1-char string for width handling
                char s[2] = { c, 0 };
                if (!print_string(&f, s)) { va_end(ap); return -1; }
            } break;

            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!print_string(&f, s)) { va_end(ap); return -1; }
            } break;

            case 'd':
            case 'i': {
                int64_t v;
                if (f.len == LEN_LL) v = va_arg(ap, long long);
                else if (f.len == LEN_L) v = va_arg(ap, long);
                else v = va_arg(ap, int);
                if (!print_signed(&f, v)) { va_end(ap); return -1; }
            } break;

            case 'u': {
                uint64_t v;
                if (f.len == LEN_LL) v = va_arg(ap, unsigned long long);
                else if (f.len == LEN_L) v = va_arg(ap, unsigned long);
                else v = va_arg(ap, unsigned int);
                if (!print_unsigned(&f, v, 10, false)) { va_end(ap); return -1; }
            } break;

            case 'x':
            case 'X': {
                if (spec == 'X') f.upper = true;
                uint64_t v;
                if (f.len == LEN_LL) v = va_arg(ap, unsigned long long);
                else if (f.len == LEN_L) v = va_arg(ap, unsigned long);
                else v = va_arg(ap, unsigned int);
                if (!print_unsigned(&f, v, 16, true)) { va_end(ap); return -1; }
            } break;

            case 'p': {
                // Always print 0x... pointer, width applies to full output
                uintptr_t p = (uintptr_t)va_arg(ap, void*);
                f.alt = true;
                if (!print_unsigned(&f, (uint64_t)p, 16, true)) { va_end(ap); return -1; }
            } break;

#if PRINTF_ENABLE_FLOAT
            case 'f': {
                double d = va_arg(ap, double);
                if (!print_float(&f, d)) { va_end(ap); return -1; }
            } break;
#endif

            default: {
                // Unknown: print literally
                if (!print_ch('%')) { va_end(ap); return -1; }
                if (spec && !print_ch(spec)) { va_end(ap); return -1; }
            } break;
        }
    }

    va_end(ap);
    return written;
}
