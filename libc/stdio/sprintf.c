#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VA_START(ap, last) __builtin_va_start(ap, last)
#define VA_END(ap)         __builtin_va_end(ap)
#define VA_ARG(ap, type)  __builtin_va_arg(ap, type)

static void buf_put(char** p, char* end, char c) {
    if (*p < end) **p = c;
    (*p)++;
}

static void buf_puts(char** p, char* end, const char* s) {
    if (!s) s = "(null)";
    while (*s) buf_put(p, end, *s++);
}

static void buf_put_uint(char** p, char* end, uint32_t v, uint32_t base) {
    char tmp[16];
    int i = 0;

    if (v == 0) {
        buf_put(p, end, '0');
        return;
    }

    while (v && i < (int)sizeof(tmp)) {
        uint32_t d = v % base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        v /= base;
    }

    while (i--) buf_put(p, end, tmp[i]);
}

static void buf_put_int(char** p, char* end, int32_t v) {
    if (v < 0) {
        buf_put(p, end, '-');
        buf_put_uint(p, end, (uint32_t)(-v), 10);
    } else {
        buf_put_uint(p, end, (uint32_t)v, 10);
    }
}

int vsnprintf(char* buf, size_t size, const char* fmt, __builtin_va_list ap) {
    char* p = buf;
    char* end = (size > 0) ? buf + size - 1 : buf;

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            buf_put(&p, end, *fmt);
            continue;
        }

        fmt++; // skip %

        switch (*fmt) {
        case '%':
            buf_put(&p, end, '%');
            break;

        case 's': {
            const char* s = VA_ARG(ap, const char*);
            buf_puts(&p, end, s);
            break;
        }

        case 'c': {
            char c = (char)VA_ARG(ap, int);
            buf_put(&p, end, c);
            break;
        }

        case 'd': {
            int v = VA_ARG(ap, int);
            buf_put_int(&p, end, v);
            break;
        }

        case 'u': {
            unsigned v = VA_ARG(ap, unsigned);
            buf_put_uint(&p, end, v, 10);
            break;
        }

        case 'x': {
            unsigned v = VA_ARG(ap, unsigned);
            buf_put_uint(&p, end, v, 16);
            break;
        }

        default:
            // unknown specifier: print literally
            buf_put(&p, end, '%');
            buf_put(&p, end, *fmt);
            break;
        }
    }

    if (size > 0) {
        if (p <= end) *p = '\0';
        else buf[end - buf + 1] = '\0';
    }

    return (int)(p - buf);
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    __builtin_va_list ap;
    VA_START(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    VA_END(ap);
    return r;
}

int sprintf(char* buf, const char* fmt, ...) {
    __builtin_va_list ap;
    VA_START(ap, fmt);
    int r = vsnprintf(buf, (size_t)-1, fmt, ap);
    VA_END(ap);
    return r;
}
