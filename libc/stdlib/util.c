// small <stdlib.h> pieces: atexit, number parsing, rand, qsort
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

int errno;

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

static void (*g_atexit[32])(void);
static int g_natexit;

int atexit(void (*fn)(void)) {
    if (g_natexit >= 32) return -1;
    g_atexit[g_natexit++] = fn;
    return 0;
}

void __run_atexit(void) {
    while (g_natexit > 0) g_atexit[--g_natexit]();
}

unsigned long strtoul(const char* s, char** end, int base) {
    const char* orig = s;
    while (isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && isxdigit((unsigned char)s[2])) { s += 2; base = 16; }
    else if (base == 0 && s[0] == '0') base = 8;
    else if (base == 0) base = 10;
    unsigned long v = 0;
    const char* start = s;
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
    }
    if (end) *end = (char*)(s == start ? orig : s);
    return neg ? -v : v;
}

long strtol(const char* s, char** end, int base) { return (long)strtoul(s, end, base); }
int atoi(const char* s) { return (int)strtol(s, 0, 10); }
long atol(const char* s) { return strtol(s, 0, 10); }

double atof(const char* s) {
    while (isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    double v = 0;
    while (isdigit((unsigned char)*s)) v = v * 10 + (*s++ - '0');
    if (*s == '.') {
        double scale = 0.1;
        for (s++; isdigit((unsigned char)*s); s++, scale /= 10) v += (*s - '0') * scale;
    }
    return neg ? -v : v;
}

static unsigned g_seed = 1;
int rand(void) {
    g_seed = g_seed * 1103515245u + 12345u;
    return (int)((g_seed >> 1) & 0x7fffffff);
}
void srand(unsigned seed) { g_seed = seed; }

char* getenv(const char* name) { (void)name; return 0; }
int system(const char* cmd) { (void)cmd; return -1; }

// shell sort: no recursion, no allocation
void qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*)) {
    char* a = base;
    char tmp[64];
    if (size > sizeof(tmp)) return;
    for (size_t gap = n / 2; gap > 0; gap /= 2) {
        for (size_t i = gap; i < n; i++) {
            memcpy(tmp, a + i * size, size);
            size_t j = i;
            while (j >= gap && cmp(a + (j - gap) * size, tmp) > 0) {
                memcpy(a + j * size, a + (j - gap) * size, size);
                j -= gap;
            }
            memcpy(a + j * size, tmp, size);
        }
    }
}

void* bsearch(const void* key, const void* base, size_t n, size_t size, int (*cmp)(const void*, const void*)) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        const char* p = (const char*)base + mid * size;
        int c = cmp(key, p);
        if (c == 0) return (void*)p;
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return 0;
}

void __assert_fail(const char* expr, const char* file, int line) {
    printf("assertion failed: %s (%s:%d)\n", expr, file, line);
    abort();
}
