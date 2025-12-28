#include <string.h>

char* strcpy(char* restrict dest, const char* restrict src) {
    char* start = dest;

    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';  // cpy null
    return start;
}
