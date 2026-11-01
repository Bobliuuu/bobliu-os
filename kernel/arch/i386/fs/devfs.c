// /dev. hd0 is the whole ata disk as a byte-addressable file; reads and
// writes that don't line up with sectors go through a bounce buffer.
#include <kernel/devfs.h>
#include <arch/i386/ata.h>
#include <string.h>
#include <stdint.h>

static const vnode_ops_t g_dir_ops;
static const vnode_ops_t g_hd_ops;

static vnode_t g_root = { .ops = &g_dir_ops, .is_dir = 1 };
static vnode_t g_hd0  = { .ops = &g_hd_ops };

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

static int dev_readdir(vnode_t* dir, uint32_t index, char* out, uint32_t max) {
    (void)dir;
    if (!ata_sectors() || index > 0) return 0;
    strncpy(out, "hd0", max - 1);
    out[max - 1] = '\0';
    return 1;
}

static vnode_t* dev_lookup(vnode_t* dir, const char* name) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return dir;
    if (ata_sectors() && strcmp(name, "hd0") == 0) return &g_hd0;
    return 0;
}

static const vnode_ops_t g_dir_ops = { .readdir = dev_readdir, .lookup = dev_lookup };
static const vnode_ops_t g_hd_ops  = { .read = hd_read, .write = hd_write };
