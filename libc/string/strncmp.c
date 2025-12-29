#include <string.h>

int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];

        if (ca != cb)
            return (int)ca - (int)cb;

        if (ca == '\0')  // both are '\0'
            return 0;
    }
    return 0;
}
