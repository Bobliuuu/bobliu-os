// printf FORMAT [ARG...]: %s %d %i %u %x %X %o %c %b %%, flags/width/precision, \n \t \\ \0NNN \xHH escapes.
// FORMAT is reused until all ARGs are consumed.
#include "common.h"
static int hexv(int c) { return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10; }

// handle one escape starting after the backslash; returns chars consumed, writes the byte, -1 = stop output
static int esc(const char* s, int* out) {
    switch (*s) {
    case 'n': *out = '\n'; return 1;
    case 't': *out = '\t'; return 1;
    case 'r': *out = '\r'; return 1;
    case 'a': *out = 7; return 1;
    case 'b': *out = 8; return 1;
    case 'f': *out = 12; return 1;
    case 'v': *out = 11; return 1;
    case 'e': *out = 27; return 1;
    case '\\': *out = '\\'; return 1;
    case 'c': *out = -1; return 1;
    case 'x': { int v = 0, n = 0; while (n < 2 && isxdigit((unsigned char)s[1 + n])) v = v * 16 + hexv(s[1 + n++]); if (!n) { *out = '\\'; return 0; } *out = v; return 1 + n; }
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
        int v = 0, n = (s[0] == '0'), cnt = 0;
        while (cnt < 3 && s[n] >= '0' && s[n] <= '7') { v = v * 8 + (s[n++] - '0'); cnt++; }
        *out = v & 255; return n;
    }
    default: *out = '\\'; return 0;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { pf("printf: usage: printf FORMAT [ARGUMENT]...\n"); return 1; }
    const char* fmt = argv[1];
    int ai = 2, rc = 0;
    for (;;) {
        int used = 0;
        for (const char* p = fmt; *p; p++) {
            if (*p == '\\') {
                int c;
                int k = esc(p + 1, &c);
                if (c < 0) DONE(rc);
                oc(c);
                p += k;
            } else if (*p == '%' && p[1] == '%') { oc('%'); p++; }
            else if (*p == '%') {
                char spec[24]; int sl = 0;
                spec[sl++] = *p++;
                while (*p && strchr("-+ #0", *p) && sl < 10) spec[sl++] = *p++;
                while (isdigit((unsigned char)*p) && sl < 16) spec[sl++] = *p++;
                if (*p == '.') { spec[sl++] = *p++; while (isdigit((unsigned char)*p) && sl < 20) spec[sl++] = *p++; }
                char conv = *p;
                if (!conv) break;
                const char* arg = ai < argc ? argv[ai] : 0;
                if (strchr("diuxXocsb", conv)) { if (arg) { ai++; used = 1; } }
                char t[600];
                switch (conv) {
                case 'd': case 'i': {
                    spec[sl++] = 'd'; spec[sl] = 0;
                    int v = 0;
                    if (arg) { char* e; v = (int)strtol(arg, &e, 0); if (*e) { err("printf", arg, "invalid number"); rc = 1; } }
                    snprintf(t, sizeof t, spec, v); put(t, strlen(t)); break;
                }
                case 'u': case 'x': case 'X': case 'o': {
                    spec[sl++] = conv; spec[sl] = 0;
                    unsigned v = 0;
                    if (arg) { char* e; v = (unsigned)strtoul(arg, &e, 0); if (*e) { err("printf", arg, "invalid number"); rc = 1; } }
                    snprintf(t, sizeof t, spec, v); put(t, strlen(t)); break;
                }
                case 'c': oc(arg && *arg ? *arg : 0); break;
                case 's': {
                    spec[sl++] = 's'; spec[sl] = 0;
                    snprintf(t, sizeof t, spec, arg ? arg : ""); put(t, strlen(t)); break;
                }
                case 'b':
                    for (const char* s = arg ? arg : ""; *s; s++) {
                        if (*s == '\\') { int c; int k = esc(s + 1, &c); if (c < 0) DONE(rc); oc(c); s += k; }
                        else oc(*s);
                    }
                    break;
                default: pf("printf: %%%c: invalid conversion\n", conv); return 1;
                }
            } else oc(*p);
        }
        if (!(ai < argc && used)) break;
    }
    DONE(rc);
}
