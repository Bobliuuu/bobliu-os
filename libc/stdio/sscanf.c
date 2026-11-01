// small sscanf: %d %i %u %x %o %s %c %n %%, optional width and '*'. whitespace in the format skips any whitespace.
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

int sscanf(const char* s, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const char* start = s;
    int count = 0;

    for (; *fmt; fmt++) {
        if (isspace((unsigned char)*fmt)) { while (isspace((unsigned char)*s)) s++; continue; }
        if (*fmt != '%') { if (*s != *fmt) goto done; s++; continue; }
        fmt++;
        int skip = 0, width = 0, lng = 0;
        if (*fmt == '*') { skip = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l' || *fmt == 'h') { if (*fmt == 'l') lng = 1; fmt++; }
        (void)lng;
        char c = *fmt;
        if (c == '%') { if (*s != '%') goto done; s++; continue; }
        if (c == 'n') { if (!skip) *va_arg(ap, int*) = (int)(s - start); continue; }
        if (c != 'c') while (isspace((unsigned char)*s)) s++;
        if (!*s) { if (count == 0) count = EOF; goto done; }

        if (c == 'c') {
            char* d = skip ? 0 : va_arg(ap, char*);
            if (d) *d = *s;
            s++;
        } else if (c == 's') {
            char* d = skip ? 0 : va_arg(ap, char*);
            int n = 0;
            while (*s && !isspace((unsigned char)*s) && (!width || n < width)) { if (d) d[n] = *s; n++; s++; }
            if (d) d[n] = 0;
        } else if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'o') {
            int base = c == 'x' || c == 'X' ? 16 : c == 'o' ? 8 : c == 'i' ? 0 : 10;
            char* end;
            unsigned long v = strtoul(s, &end, base);
            if (end == s) goto done;
            if (width && end - s > width) { goto done; }   // widths on numbers are not supported
            s = end;
            if (!skip) *va_arg(ap, unsigned*) = (unsigned)v;
        } else goto done;
        if (!skip) count++;
    }
done:
    va_end(ap);
    return count;
}
