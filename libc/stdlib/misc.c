// locale, strerror, strcoll, tmpnam: stubs for the bits ported software expects
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#define getpid_ getpid

char* setlocale(int cat, const char* loc) {
    (void)cat;
    if (!loc || !*loc || !strcmp(loc, "C") || !strcmp(loc, "POSIX")) return "C";
    return 0;
}

int strcoll(const char* a, const char* b) { return strcmp(a, b); }

char* strerror(int e) {
    switch (e) {
    case 0: return "Success";
    case ENOENT: return "No such file or directory";
    case EIO: return "Input/output error";
    case EBADF: return "Bad file descriptor";
    case ENOMEM: return "Cannot allocate memory";
    case EACCES: return "Permission denied";
    case EEXIST: return "File exists";
    case EISDIR: return "Is a directory";
    case EINVAL: return "Invalid argument";
    case ERANGE: return "Numerical result out of range";
    }
    return "Unknown error";
}

char* tmpnam(char* buf) {
    static char sbuf[L_tmpnam];
    static int n;
    char* b = buf ? buf : sbuf;
    snprintf(b, L_tmpnam, "/tmp/tmp%d_%d", getpid_(), n++);
    return b;
}

FILE* tmpfile(void) {
    char name[L_tmpnam];
    tmpnam(name);
    FILE* f = fopen(name, "w+");
    if (f) remove(name);        // unlink-while-open is supported on /tmp
    return f;
}

struct lconv* localeconv(void) {
    static struct lconv lc = { ".", "", "" };
    return &lc;
}
