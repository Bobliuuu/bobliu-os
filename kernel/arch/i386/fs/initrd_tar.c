#include <kernel/initrd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

static const uint8_t* g_initrd = 0;
static size_t g_initrd_len = 0;

typedef struct __attribute__((packed)) {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];    // "ustar"
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} tar_hdr_t;

static unsigned oct2u(const char* s, size_t n) {
    unsigned v = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\0' || c == ' ') break;
        if (c < '0' || c > '7') break;
        v = (v << 3) + (unsigned)(c - '0');
    }
    return v;
}

static int is_zero_block(const uint8_t* p) {
    for (int i = 0; i < 512; i++) if (p[i] != 0) return 0;
    return 1;
}

void initrd_init_from_module(uintptr_t start, uintptr_t end) {
    g_initrd = (const uint8_t*)start;
    g_initrd_len = (size_t)(end - start);
    printf("initrd: %x..%x (%u bytes)\n", (uint32_t)start, (uint32_t)end, (uint32_t)g_initrd_len);
}

void initrd_ls(void) {
    if (!g_initrd) { printf("initrd not loaded\n"); return; }

    size_t off = 0;
    while (off + 512 <= g_initrd_len) {
        const uint8_t* blk = g_initrd + off;
        if (is_zero_block(blk)) break;

        const tar_hdr_t* h = (const tar_hdr_t*)blk;
        unsigned sz = oct2u(h->size, sizeof(h->size));

        // build full name: prefix/name if prefix exists
        char full[256];
        if (h->prefix[0]) {
            snprintf(full, sizeof(full), "%s/%s", h->prefix, h->name);
        } else {
            snprintf(full, sizeof(full), "%s", h->name);
        }

        // skip empty names
        if (full[0] != '\0') {
            if (h->typeflag == '5') {
                printf("[dir ] %s\n", full);
            } else {
                printf("[file] %s (%u)\n", full, sz);
            }
        }

        // advance: header + file data rounded to 512
        size_t data = ((size_t)sz + 511) & ~511u;
        off += 512 + data;
    }
}

int initrd_cat(const char* path) {
    if (!g_initrd) { printf("initrd not loaded\n"); return -1; }

    size_t off = 0;
    while (off + 512 <= g_initrd_len) {
        const uint8_t* blk = g_initrd + off;
        if (is_zero_block(blk)) break;

        const tar_hdr_t* h = (const tar_hdr_t*)blk;
        unsigned sz = oct2u(h->size, sizeof(h->size));

        char full[256];
        if (h->prefix[0]) snprintf(full, sizeof(full), "%s/%s", h->prefix, h->name);
        else snprintf(full, sizeof(full), "%s", h->name);

        const uint8_t* data = blk + 512;

        if (h->typeflag != '5' && strcmp(full, path) == 0) {
            // print as text
            for (unsigned i = 0; i < sz; i++) {
                char c = (char)data[i];
                putchar(c);
            }
            if (sz && ((char)data[sz-1] != '\n')) putchar('\n');
            return 0;
        }

        size_t adv = ((size_t)sz + 511) & ~511u;
        off += 512 + adv;
    }

    printf("not found: %s\n", path);
    return -1;
}
