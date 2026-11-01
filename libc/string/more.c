// the rest of <string.h>
#include <string.h>
#include <stdlib.h>

char* strrchr(const char* s, int c) {
    const char* last = 0;
    do { if (*s == (char)c) last = s; } while (*s++);
    return (char*)last;
}

#if !defined(__is_libk)
char* strdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
#endif

char* strncat(char* dst, const char* src, size_t n) {
    char* d = dst + strlen(dst);
    while (n-- && *src) *d++ = *src++;
    *d = 0;
    return dst;
}

void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = s;
    for (; n; n--, p++) if (*p == (unsigned char)c) return (void*)p;
    return 0;
}

size_t strspn(const char* s, const char* accept) {
    size_t n = 0;
    while (s[n] && strchr(accept, s[n])) n++;
    return n;
}

size_t strcspn(const char* s, const char* reject) {
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) n++;
    return n;
}

char* strpbrk(const char* s, const char* accept) {
    s += strcspn(s, accept);
    return *s ? (char*)s : 0;
}

char* strtok(char* s, const char* delim) {
    static char* save;
    if (!s) s = save;
    if (!s) return 0;
    s += strspn(s, delim);
    if (!*s) { save = 0; return 0; }
    char* end = s + strcspn(s, delim);
    if (*end) { *end = 0; save = end + 1; } else save = 0;
    return s;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int strcasecmp(const char* a, const char* b) {
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) { a++; b++; }
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

int strncasecmp(const char* a, const char* b, size_t n) {
    while (n && *a && lower((unsigned char)*a) == lower((unsigned char)*b)) { a++; b++; n--; }
    return n ? lower((unsigned char)*a) - lower((unsigned char)*b) : 0;
}
