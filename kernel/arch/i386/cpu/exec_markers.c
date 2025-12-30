#include <stdint.h>
#include <stdio.h>

volatile uint32_t g_marker = 0;

void mark(uint32_t x) {
    g_marker = x;
    printf("[MARK %x]\n", x);
}
