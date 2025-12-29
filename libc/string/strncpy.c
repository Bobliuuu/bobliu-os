#include <string.h>

char* strncpy(char* dst, const char* src, size_t n) {
    size_t i = 0;

    // copy until src ends or we hit n
    for (; i < n && src[i]; i++) {
        dst[i] = src[i];
    }

    // pad with '\0' up to n
    for (; i < n; i++) {
        dst[i] = '\0';
    }

    return dst;
}
