#include <kernel/initrd_vfs.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;   // '0' file, '5' dir
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} tar_hdr_t;

static const uint8_t* g_tar = 0;
static size_t g_tar_len = 0;

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

static void make_full(char* out, size_t out_sz, const tar_hdr_t* h) {
    size_t i = 0;
    if (h->prefix[0]) {
        for (size_t j = 0; h->prefix[j] && i + 1 < out_sz; j++) out[i++] = h->prefix[j];
        if (i + 1 < out_sz) out[i++] = '/';
    }
    for (size_t j = 0; h->name[j] && i + 1 < out_sz; j++) out[i++] = h->name[j];
    out[i] = '\0';

    // normalize "./foo" -> "foo"
    if (out[0] == '.' && out[1] == '/') {
        size_t L = strlen(out);
        memmove(out, out + 2, L - 1); // includes null
    }

    // normalize trailing "/" for dirs if you want consistency
}

static int starts_with_dir(const char* full, const char* dir_prefix) {
    // dir_prefix is "" for root, or "dir/sub"
    if (!dir_prefix || !dir_prefix[0]) return 1;

    size_t n = strlen(dir_prefix);
    if (strncmp(full, dir_prefix, n) != 0) return 0;
    if (full[n] == '\0') return 1;
    return full[n] == '/';
}

static const uint8_t* tar_find_exact(const char* path, tar_hdr_t** out_hdr) {
    size_t off = 0;
    char full[256];

    while (off + 512 <= g_tar_len) {
        const uint8_t* blk = g_tar + off;
        if (is_zero_block(blk)) break;

        tar_hdr_t* h = (tar_hdr_t*)blk;
        unsigned sz = oct2u(h->size, sizeof(h->size));
        make_full(full, sizeof(full), h);

        // tar often stores dirs with trailing '/'
        // normalize: if header is dir and ends with '/', compare without it too
        if (strcmp(full, path) == 0) {
            *out_hdr = h;
            return blk;
        }
        if (h->typeflag == '5') {
            size_t L = strlen(full);
            if (L && full[L-1] == '/') {
                full[L-1] = '\0';
                if (strcmp(full, path) == 0) {
                    *out_hdr = h;
                    return blk;
                }
            }
        }

        size_t adv = ((size_t)sz + 511) & ~511u;
        off += 512 + adv;
    }

    return 0;
}

// vnode fs_data types
typedef struct {
    char prefix[128]; // directory prefix ("" root, or "dir/sub")
} dir_data_t;

static int initrd_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len);
static int initrd_readdir(vnode_t* vn, uint32_t index, char* name_out, uint32_t name_max);
static vnode_t* initrd_lookup(vnode_t* dir, const char* name);

static const vnode_ops_t g_ops = {
    .read = initrd_read,
    .readdir = initrd_readdir,
    .lookup = initrd_lookup
};

static vnode_t* g_root = 0;

static vnode_t* vnode_make_dir(const char* prefix) {
    vnode_t* vn = (vnode_t*)kmalloc(sizeof(vnode_t));
    dir_data_t* dd = (dir_data_t*)kmalloc(sizeof(dir_data_t));

    dd->prefix[0] = '\0';
    if (prefix && prefix[0]) {
        // copy (truncate)
        size_t n = strlen(prefix);
        if (n >= sizeof(dd->prefix)) n = sizeof(dd->prefix) - 1;
        memcpy(dd->prefix, prefix, n);
        dd->prefix[n] = '\0';
    }

    vn->ops = &g_ops;
    vn->fs_data = dd;
    vn->size = 0;
    vn->is_dir = 1;
    return vn;
}

static vnode_t* vnode_make_file(tar_hdr_t* h, uint32_t size) {
    vnode_t* vn = (vnode_t*)kmalloc(sizeof(vnode_t));
    vn->ops = &g_ops;
    vn->fs_data = h;  // points into tar image in memory
    vn->size = size;
    vn->is_dir = 0;
    return vn;
}

void initrd_vfs_init(uintptr_t start, uintptr_t end) {
    g_tar = (const uint8_t*)start;
    g_tar_len = (size_t)(end - start);
    g_root = vnode_make_dir(""); // root prefix
    printf("initrd_vfs: %x..%x\n", (uint32_t)start, (uint32_t)end);
    //tar_hdr_t* h = (tar_hdr_t*)g_tar;
    //printf("tar[0] name='%s' prefix='%s'\n", h->name, h->prefix);
    //printf("tar[0] magic='%.5s'\n", h->magic);
}

vnode_t* initrd_vfs_root(void) {
    return g_root;
}

static int initrd_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) {
    if (!vn || vn->is_dir) return -1;
    tar_hdr_t* h = (tar_hdr_t*)vn->fs_data;
    uint32_t sz = vn->size;
    if (off >= sz) return 0;

    uint32_t n = len;
    if (off + n > sz) n = sz - off;

    const uint8_t* data = (const uint8_t*)h + 512;
    memcpy(buf, data + off, n);
    return (int)n;
}

// directory listing: return immediate children only
static int initrd_readdir(vnode_t* vn, uint32_t index, char* name_out, uint32_t name_max) {
    if (!vn || !vn->is_dir) return -1;
    dir_data_t* dd = (dir_data_t*)vn->fs_data;

    //printf("readdir prefix='%s'\n", dd->prefix);
    //printf("initrd_len=%u\n", (uint32_t)g_tar_len);

    size_t off = 0;
    char full[256];

    // we must return unique immediate entries. We'll do a simple "skip duplicates" by rescanning.
    uint32_t found = 0;

    while (off + 512 <= g_tar_len) {
        const uint8_t* blk = g_tar + off;
        if (is_zero_block(blk)) break;

        tar_hdr_t* h = (tar_hdr_t*)blk;
        unsigned sz = oct2u(h->size, sizeof(h->size));
        make_full(full, sizeof(full), h);

        if (full[0] == '\0') goto adv;

        // must be under directory prefix
        if (!starts_with_dir(full, dd->prefix)) goto adv;

        // compute remainder after prefix
        const char* rel = full;
        if (dd->prefix[0]) {
            rel = full + strlen(dd->prefix);
            if (*rel == '/') rel++;
        }

        // immediate child = up to next '/'
        if (!rel[0]) goto adv;

        char child[128];
        size_t i = 0;
        while (rel[i] && rel[i] != '/' && i + 1 < sizeof(child)) {
            child[i] = rel[i];
            i++;
        }
        child[i] = '\0';

        // skip duplicates: check if this child already appeared earlier
        bool dup = false;
        size_t off2 = 0;
        while (off2 < off) {
            const uint8_t* b2 = g_tar + off2;
            tar_hdr_t* h2 = (tar_hdr_t*)b2;
            unsigned sz2 = oct2u(h2->size, sizeof(h2->size));
            char full2[256];
            make_full(full2, sizeof(full2), h2);

            if (starts_with_dir(full2, dd->prefix)) {
                const char* rel2 = full2;
                if (dd->prefix[0]) {
                    rel2 = full2 + strlen(dd->prefix);
                    if (*rel2 == '/') rel2++;
                }
                if (rel2[0]) {
                    char child2[128];
                    size_t j = 0;
                    while (rel2[j] && rel2[j] != '/' && j + 1 < sizeof(child2)) {
                        child2[j] = rel2[j];
                        j++;
                    }
                    child2[j] = '\0';
                    if (strcmp(child2, child) == 0) { dup = true; break; }
                }
            }

            size_t adv2 = ((size_t)sz2 + 511) & ~511u;
            off2 += 512 + adv2;
        }
        if (dup) goto adv;

        if (found == index) {
            size_t n = strlen(child);
            if (n >= name_max) n = name_max - 1;
            memcpy(name_out, child, n);
            name_out[n] = '\0';
            return 1;
        }
        found++;

    adv:
        {
            size_t adv = ((size_t)sz + 511) & ~511u;
            off += 512 + adv;
        }
    }

    return 0; // end
}

static vnode_t* initrd_lookup(vnode_t* dir, const char* name) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return dir;
    if (!dir || !dir->is_dir) return 0;
    dir_data_t* dd = (dir_data_t*)dir->fs_data;

    // build target path: prefix + "/" + name
    char target[256];
    if (dd->prefix[0]) {
        // prefix/name
        size_t p = strlen(dd->prefix);
        size_t i = 0;
        while (dd->prefix[i] && i + 1 < sizeof(target)) { target[i] = dd->prefix[i]; i++; }
        if (i + 1 < sizeof(target)) target[i++] = '/';
        size_t j = 0;
        while (name[j] && i + 1 < sizeof(target)) target[i++] = name[j++];
        target[i] = '\0';
    } else {
        // name
        size_t i = 0;
        while (name[i] && i + 1 < sizeof(target)) { target[i] = name[i]; i++; }
        target[i] = '\0';
    }

    // If exact file exists, return file vnode
    tar_hdr_t* h = 0;
    const uint8_t* blk = tar_find_exact(target, &h);
    if (blk && h) {
        unsigned sz = oct2u(h->size, sizeof(h->size));
        if (h->typeflag == '5') return vnode_make_dir(target);
        return vnode_make_file(h, (uint32_t)sz);
    }

    // Otherwise, treat as directory if anything exists under "target/"
    // (so directories don't need explicit tar dir entries)
    char prefix2[260];
    size_t k = 0;
    while (target[k] && k + 2 < sizeof(prefix2)) { prefix2[k] = target[k]; k++; }
    prefix2[k++] = '/';
    prefix2[k] = '\0';

    size_t off = 0;
    char full[256];
    while (off + 512 <= g_tar_len) {
        const uint8_t* b = g_tar + off;
        if (is_zero_block(b)) break;
        tar_hdr_t* hh = (tar_hdr_t*)b;
        unsigned sz = oct2u(hh->size, sizeof(hh->size));
        make_full(full, sizeof(full), hh);

        if (strncmp(full, prefix2, strlen(prefix2)) == 0) {
            return vnode_make_dir(target);
        }

        size_t adv = ((size_t)sz + 511) & ~511u;
        off += 512 + adv;
    }

    return 0;
}
