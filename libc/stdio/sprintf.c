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


// ---- floating point: exact decimal expansion with a small base-1e9 bignum, then round half to even ----
#define BIGN 90
typedef struct { uint32_t d[BIGN]; int n; } big_t;      // little endian limbs, base 1e9

static void big_mul(big_t* b, uint32_t m) {
    uint64_t carry = 0;
    for (int i = 0; i < b->n; i++) {
        uint64_t v = (uint64_t)b->d[i] * m + carry;
        b->d[i] = (uint32_t)(v % 1000000000u);
        carry = v / 1000000000u;
    }
    while (carry && b->n < BIGN) { b->d[b->n++] = (uint32_t)(carry % 1000000000u); carry /= 1000000000u; }
}

// digits of |v| (finite, != 0) into digs (no leading zeros), value = 0.DIGS * 10^dp. returns number of digits
static int exact_digits(double v, char* digs, int* dp) {
    union { double f; uint64_t u; } x = { v };
    int e2 = (int)((x.u >> 52) & 0x7ff);
    uint64_t m = x.u & 0xfffffffffffffull;
    if (e2) m |= 1ull << 52; else e2 = 1;
    e2 -= 1075;                                  // v = m * 2^e2
    while (!(m & 1) && e2 < 0) { m >>= 1; e2++; }

    big_t b;
    b.n = 0;
    b.d[b.n++] = (uint32_t)(m % 1000000000u);
    if (m >= 1000000000u) b.d[b.n++] = (uint32_t)((m / 1000000000u) % 1000000000u);
    if (m >= 1000000000000000000ull) b.d[b.n++] = (uint32_t)(m / 1000000000000000000ull);
    int frac = 0;                                // number of decimal fraction digits in b
    if (e2 > 0) { for (int i = 0; i < e2; i++) big_mul(&b, 2); }
    else if (e2 < 0) {
        // m / 2^k = m * 5^k / 10^k
        for (int i = 0; i < -e2; i++) big_mul(&b, 5);
        frac = -e2;
    }
    int nd = 0;
    char tmp[BIGN * 9 + 1];
    for (int i = b.n - 1; i >= 0; i--) {
        uint32_t limb = b.d[i];
        char t[9];
        for (int k = 8; k >= 0; k--) { t[k] = '0' + limb % 10; limb /= 10; }
        for (int k = 0; k < 9; k++) tmp[nd++] = t[k];
    }
    int lead = 0;
    while (lead < nd - 1 && tmp[lead] == '0') lead++;
    int len = nd - lead;
    for (int i = 0; i < len; i++) digs[i] = tmp[lead + i];
    *dp = len - frac;                            // decimal point position relative to the first digit
    while (len > 1 && digs[len - 1] == '0') len--;
    return len;
}

// keep the first 'keep' digits (may be <= 0), rounding half to even on the exact value. returns new count (== max(keep,0), zero padded)
static int round_digits(char* digs, int nd, int keep, int* dp) {
    if (keep >= nd) { for (int i = nd; i < keep; i++) digs[i] = '0'; return keep; }
    int up = 0;
    if (keep >= 0) {
        char d = digs[keep];
        if (d > '5') up = 1;
        else if (d == '5') {
            int rest = 0;
            for (int i = keep + 1; i < nd; i++) if (digs[i] != '0') rest = 1;
            up = rest || (keep > 0 && ((digs[keep - 1] - '0') & 1));
        }
    }
    if (keep < 0) keep = 0;
    if (up) {
        int i = keep - 1;
        while (i >= 0 && digs[i] == '9') { digs[i] = '0'; i--; }
        if (i >= 0) digs[i]++;
        else {                                   // 999.. -> 1000..
            for (int k = keep; k > 0; k--) digs[k] = digs[k - 1];
            digs[0] = '1';
            (*dp)++;
            keep++;
        }
    }
    return keep;
}

// writes the formatted number (no sign) into out, returns length. conv: 'f','e','g' (+ upper via flag)
static int fmt_double(char* out, double v, char conv, int prec, bool alt, bool upper) {
    int o = 0;
    char digs[BIGN * 9 + 8];
    int nd, dp;
    if (prec < 0) prec = 6;
    if (v == 0) { digs[0] = '0'; nd = 1; dp = 1; }
    else nd = exact_digits(v, digs, &dp);
    bool zero = v == 0;

    if (conv == 'g') {
        int P = prec == 0 ? 1 : prec;
        int keep = P;
        int dp2 = dp;
        int n2 = zero ? 1 : round_digits(digs, nd, keep, &dp2);
        int X = zero ? 0 : dp2 - 1;
        if (X < -4 || X >= P) { conv = 'e'; prec = P - 1; }
        else { conv = 'f'; prec = P - 1 - X; }
        if (!alt) {
            // trailing zeros are removed: shrink the precision to the significant digits
            int sig = n2;
            while (sig > 1 && digs[sig - 1] == '0') sig--;
            if (conv == 'e') prec = sig - 1;
            else { int p2 = sig - dp2; prec = p2 < 0 ? 0 : p2; }
            if (zero) prec = 0;
        }
        nd = n2; dp = dp2;                       // already rounded to P digits; re-rounding below is a no-op
    }

    if (conv == 'e') {
        int dp2 = dp;
        int n2 = zero ? 1 : round_digits(digs, nd, prec + 1, &dp2);
        if (zero) { for (int i = 1; i <= prec; i++) digs[i] = '0'; n2 = prec + 1; }
        out[o++] = digs[0];
        if (prec > 0 || alt) out[o++] = '.';
        for (int i = 1; i <= prec; i++) out[o++] = i < n2 ? digs[i] : '0';
        int X = zero ? 0 : dp2 - 1;
        out[o++] = upper ? 'E' : 'e';
        out[o++] = X < 0 ? '-' : '+';
        if (X < 0) X = -X;
        if (X >= 100) out[o++] = '0' + X / 100;
        out[o++] = '0' + (X / 10) % 10;
        out[o++] = '0' + X % 10;
        return o;
    }

    // 'f'
    int dp2 = dp;
    int n2;
    if (zero) { n2 = 0; dp2 = 1; }
    else n2 = round_digits(digs, nd, dp + prec, &dp2);   // digits up to prec places after the point
    // integer part
    if (dp2 <= 0) out[o++] = '0';
    else for (int i = 0; i < dp2; i++) out[o++] = i < n2 ? digs[i] : '0';
    if (prec > 0 || alt) out[o++] = '.';
    for (int i = 0; i < prec; i++) {
        int idx = dp2 + i;                       // index into digs (leading zeros when idx < 0)
        out[o++] = (idx >= 0 && idx < n2) ? digs[idx] : '0';
    }
    return o;
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
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
            double v = __builtin_va_arg(ap, double);
            bool neg = __builtin_signbit(v);
            if (neg) v = -v;
            char body[BIGN * 9 + 40];
            int bl;
            if (v != v) { body[0] = 'n'; body[1] = 'a'; body[2] = 'n'; bl = 3; zero = false; if (*fmt < 'a') { body[0] = 'N'; body[1] = 'A'; body[2] = 'N'; } }
            else if (v > 1.7976931348623157e308) { body[0] = 'i'; body[1] = 'n'; body[2] = 'f'; bl = 3; zero = false; if (*fmt < 'a') { body[0] = 'I'; body[1] = 'N'; body[2] = 'F'; } }
            else bl = fmt_double(body, v, *fmt | 0x20, prec, alt, *fmt < 'a');
            char sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;
            int len = bl + (sign ? 1 : 0);
            if (!left && !zero) pad(&o, ' ', width - len);
            if (sign) put(&o, sign);
            if (!left && zero) pad(&o, '0', width - len);
            for (int i = 0; i < bl; i++) put(&o, body[i]);
            if (left) pad(&o, ' ', width - len);
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
