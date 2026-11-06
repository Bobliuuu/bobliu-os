// /dev. hd0 is the whole ata disk as a byte-addressable file; reads and
// writes that don't line up with sectors go through a bounce buffer.
#include <kernel/devfs.h>
#include <arch/i386/ata.h>
#include <arch/i386/timer.h>
#include <string.h>
#include <stdint.h>

static const vnode_ops_t g_dir_ops;
static const vnode_ops_t g_hd_ops;
static const vnode_ops_t g_null_ops, g_zero_ops, g_rand_ops;

static vnode_t g_root = { .ops = &g_dir_ops, .is_dir = 1 };
static vnode_t g_hd0  = { .ops = &g_hd_ops };
static vnode_t g_null = { .ops = &g_null_ops, .kind = VK_CHR };
static vnode_t g_zero = { .ops = &g_zero_ops, .kind = VK_CHR };
static vnode_t g_rand = { .ops = &g_rand_ops, .kind = VK_CHR };

vnode_t* devfs_create_root(void) {
    g_hd0.size = ata_sectors() * ATA_SECTOR;
    return &g_root;
}

static int hd_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) {
    if (off >= vn->size) return 0;
    if (len > vn->size - off) len = vn->size - off;

    uint8_t sec[ATA_SECTOR];
    uint8_t* out = (uint8_t*)buf;
    uint32_t done = 0;
    while (done < len) {
        uint32_t pos = off + done;
        uint32_t in_sec = pos % ATA_SECTOR;
        uint32_t n = ATA_SECTOR - in_sec;
        if (n > len - done) n = len - done;
        if (ata_read(pos / ATA_SECTOR, sec) < 0) return done ? (int)done : -1;
        memcpy(out + done, sec + in_sec, n);
        done += n;
    }
    return (int)done;
}

static int hd_write(vnode_t* vn, uint32_t off, const void* buf, uint32_t len) {
    if (off >= vn->size) return -1;
    if (len > vn->size - off) len = vn->size - off;

    uint8_t sec[ATA_SECTOR];
    const uint8_t* in = (const uint8_t*)buf;
    uint32_t done = 0;
    while (done < len) {
        uint32_t pos = off + done;
        uint32_t in_sec = pos % ATA_SECTOR;
        uint32_t n = ATA_SECTOR - in_sec;
        if (n > len - done) n = len - done;
        // partial sector: read-modify-write
        if (n != ATA_SECTOR && ata_read(pos / ATA_SECTOR, sec) < 0) return done ? (int)done : -1;
        memcpy(sec + in_sec, in + done, n);
        if (ata_write(pos / ATA_SECTOR, sec) < 0) return done ? (int)done : -1;
        done += n;
    }
    return (int)done;
}

/* /dev/null swallows writes and reads as empty; /dev/zero reads zeros; /dev/urandom (and
   /dev/random) read xorshift output seeded from the tick counter. */
static int null_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) { (void)vn; (void)off; (void)buf; (void)len; return 0; }
static int null_write(vnode_t* vn, uint32_t off, const void* buf, uint32_t len) { (void)vn; (void)off; (void)buf; return (int)len; }
static int zero_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) { (void)vn; (void)off; memset(buf, 0, len); return (int)len; }
static int rand_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) {
    (void)vn; (void)off;
    static uint32_t x;
    if (!x) x = 2463534242u ^ timer_ticks();
    uint8_t* o = (uint8_t*)buf;
    for (uint32_t i = 0; i < len; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        o[i] = (uint8_t)(x >> 8);
    }
    return (int)len;
}

static const char* const g_names[] = { "hd0", "null", "zero", "urandom", "random" };

static int dev_readdir(vnode_t* dir, uint32_t index, char* out, uint32_t max) {
    (void)dir;
    uint32_t n = 0;
    for (uint32_t i = 0; i < sizeof g_names / sizeof *g_names; i++) {
        if (i == 0 && !ata_sectors()) continue;     // no disk, no hd0
        if (n++ == index) {
            strncpy(out, g_names[i], max - 1);
            out[max - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

static vnode_t* dev_lookup(vnode_t* dir, const char* name) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return dir;
    if (ata_sectors() && strcmp(name, "hd0") == 0) return &g_hd0;
    if (strcmp(name, "null") == 0) return &g_null;
    if (strcmp(name, "zero") == 0) return &g_zero;
    if (strcmp(name, "urandom") == 0 || strcmp(name, "random") == 0) return &g_rand;
    return 0;
}

static const vnode_ops_t g_dir_ops = { .readdir = dev_readdir, .lookup = dev_lookup };
static const vnode_ops_t g_hd_ops  = { .read = hd_read, .write = hd_write };
static const vnode_ops_t g_null_ops = { .read = null_read, .write = null_write };
static const vnode_ops_t g_zero_ops = { .read = zero_read, .write = null_write };
static const vnode_ops_t g_rand_ops = { .read = rand_read, .write = null_write };
