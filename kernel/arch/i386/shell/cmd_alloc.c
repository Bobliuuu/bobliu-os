#include <kernel/shell.h>
#include <kernel/heap.h>
#include <stdint.h>
#include <stddef.h>

extern int printf(const char*, ...);

static uint32_t parse_u32(const char* s) {
    uint32_t x = 0;
    while (*s >= '0' && *s <= '9') {
        x = x * 10u + (uint32_t)(*s - '0');
        s++;
    }
    return x;
}

int cmd_alloc(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: alloc <bytes>\n");
        return 0;
    }

    uint32_t n = parse_u32(argv[1]);
    void* p = kmalloc(n);
    printf("kmalloc(%u) = %x\n", n, (uint32_t)(uintptr_t)p);
    printf("heap used %u / %u bytes\n",
           (uint32_t)heap_bytes_used(), (uint32_t)heap_bytes_total());
    return 0;
}
