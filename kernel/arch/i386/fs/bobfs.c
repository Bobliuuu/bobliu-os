// bobfs layout (1 KiB blocks, block = 2 sectors):
//   0 superblock | 1 inode bitmap | 2 data bitmap | 3.. inode table (64-byte inodes) | data
// an inode has 12 direct blocks and one indirect block (max file ~268 KiB).
// directories are files of 32-byte entries {ino, name[28]}; ino 0 = free slot.
// everything is write-through, there is no cache and no journal.
#include <kernel/bobfs.h>
#include <arch/i386/ata.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define BLK        1024u
#define NINODES    128u
#define NDIRECT    12u
#define NINDIRECT  (BLK / 4)
#define MAGIC      0x424F4246u     /* "BOBF" */
#define T_FILE     1
#define T_DIR      2
#define NAME_LEN   28
#define ROOT_INO   1

struct sb {
    uint32_t magic, nblocks, ninodes;
    uint32_t ibmap, dbmap, itable, itable_blocks, data_start;
};

struct dinode {
    uint16_t type, pad;
    uint32_t size;
    uint32_t direct[NDIRECT];
    uint32_t indirect;
    uint32_t pad2;
};

struct dent {
    uint32_t ino;
    char name[NAME_LEN];
};

#define IPB (BLK / sizeof(struct dinode))

static struct sb g_sb;
static int g_ok = 0;
static vnode_t g_vn[NINODES];
static uint8_t g_unlinked[NINODES];
static const vnode_ops_t g_ops;

// ---- block io --------------------------------------------------------------

static int blk_read(uint32_t b, void* buf) {
    uint8_t* p = (uint8_t*)buf;
    if (ata_read(b * 2, p) < 0 || ata_read(b * 2 + 1, p + 512) < 0) return -1;
    return 0;
}

static int blk_write(uint32_t b, const void* buf) {
    const uint8_t* p = (const uint8_t*)buf;
    if (ata_write(b * 2, p) < 0 || ata_write(b * 2 + 1, p + 512) < 0) return -1;
    return 0;
}

static int inode_read(uint32_t ino, struct dinode* di) {
    uint8_t buf[BLK];
    if (ino == 0 || ino >= NINODES || blk_read(g_sb.itable + ino / IPB, buf) < 0) return -1;
    *di = ((struct dinode*)buf)[ino % IPB];
    return 0;
}

static int inode_write(uint32_t ino, const struct dinode* di) {
    uint8_t buf[BLK];
    uint32_t b = g_sb.itable + ino / IPB;
    if (blk_read(b, buf) < 0) return -1;
    ((struct dinode*)buf)[ino % IPB] = *di;
    return blk_write(b, buf);
}

// ---- bitmaps ---------------------------------------------------------------

static int bit_alloc(uint32_t map_blk, uint32_t first, uint32_t limit) {
    uint8_t buf[BLK];
    if (blk_read(map_blk, buf) < 0) return 0;
    for (uint32_t i = first; i < limit; i++) {
        if (!(buf[i / 8] & (1u << (i % 8)))) {
            buf[i / 8] |= (uint8_t)(1u << (i % 8));
            return blk_write(map_blk, buf) < 0 ? 0 : (int)i;
        }
    }
    return 0;
}

static void bit_free(uint32_t map_blk, uint32_t i) {
    uint8_t buf[BLK];
    if (blk_read(map_blk, buf) < 0) return;
    buf[i / 8] &= (uint8_t)~(1u << (i % 8));
    blk_write(map_blk, buf);
}

static uint32_t balloc(void) {
    uint32_t b = (uint32_t)bit_alloc(g_sb.dbmap, g_sb.data_start, g_sb.nblocks);
    if (b) { uint8_t z[BLK]; memset(z, 0, BLK); blk_write(b, z); }
    return b;
}

static void bfree(uint32_t b) { bit_free(g_sb.dbmap, b); }

// ---- file blocks -----------------------------------------------------------

// disk block holding file block `fb`, 0 if it isn't there (and alloc == 0 or no space)
static uint32_t bmap(struct dinode* di, uint32_t fb, int alloc, int* dirty) {
    if (fb < NDIRECT) {
        if (!di->direct[fb] && alloc && (di->direct[fb] = balloc())) *dirty = 1;
        return di->direct[fb];
    }
    fb -= NDIRECT;
    if (fb >= NINDIRECT) return 0;
    if (!di->indirect) {
        if (!alloc || !(di->indirect = balloc())) return 0;
        *dirty = 1;
    }
    uint32_t tbl[NINDIRECT];
    if (blk_read(di->indirect, tbl) < 0) return 0;
    if (!tbl[fb] && alloc && (tbl[fb] = balloc())) blk_write(di->indirect, tbl);
    return tbl[fb];
}

static int file_read(uint32_t ino, uint32_t off, void* buf, uint32_t len) {
    struct dinode di;
    if (inode_read(ino, &di) < 0) return -1;
    if (off >= di.size) return 0;
    if (len > di.size - off) len = di.size - off;

    uint8_t blk[BLK];
    uint32_t done = 0;
    while (done < len) {
        uint32_t pos = off + done, in = pos % BLK, n = BLK - in;
        if (n > len - done) n = len - done;
        int dirty = 0;
        uint32_t b = bmap(&di, pos / BLK, 0, &dirty);
        if (b) { if (blk_read(b, blk) < 0) return done ? (int)done : -1; }
        else memset(blk, 0, BLK);                      // hole
        memcpy((uint8_t*)buf + done, blk + in, n);
        done += n;
    }
    return (int)done;
}

static int file_write(uint32_t ino, uint32_t off, const void* buf, uint32_t len) {
    struct dinode di;
    if (inode_read(ino, &di) < 0) return -1;

    uint8_t blk[BLK];
    uint32_t done = 0;
    int dirty = 0;
    while (done < len) {
        uint32_t pos = off + done, in = pos % BLK, n = BLK - in;
        if (n > len - done) n = len - done;
        uint32_t b = bmap(&di, pos / BLK, 1, &dirty);
        if (!b) break;                                  // disk full / file too big
        if (n != BLK && blk_read(b, blk) < 0) break;
        memcpy(blk + in, (const uint8_t*)buf + done, n);
        if (blk_write(b, blk) < 0) break;
        done += n;
    }
    if (done && off + done > di.size) { di.size = off + done; dirty = 1; }
    if (dirty) inode_write(ino, &di);
    return done ? (int)done : (len ? -1 : 0);
}

static void file_truncate(uint32_t ino) {
    struct dinode di;
    if (inode_read(ino, &di) < 0) return;
    for (uint32_t i = 0; i < NDIRECT; i++) if (di.direct[i]) { bfree(di.direct[i]); di.direct[i] = 0; }
    if (di.indirect) {
        uint32_t tbl[NINDIRECT];
        if (blk_read(di.indirect, tbl) == 0)
            for (uint32_t i = 0; i < NINDIRECT; i++) if (tbl[i]) bfree(tbl[i]);
        bfree(di.indirect);
        di.indirect = 0;
    }
    di.size = 0;
    inode_write(ino, &di);
}

// ---- directories -----------------------------------------------------------

// find name in dir; returns ino (0 if absent) and the entry's byte offset
static uint32_t dir_find(uint32_t dir, const char* name, uint32_t* at) {
    struct dent e;
    for (uint32_t off = 0; file_read(dir, off, &e, sizeof(e)) == (int)sizeof(e); off += sizeof(e)) {
        if (e.ino && strncmp(e.name, name, NAME_LEN) == 0) { if (at) *at = off; return e.ino; }
    }
    return 0;
}

static int dir_add(uint32_t dir, const char* name, uint32_t ino) {
    struct dent e;
    uint32_t off = 0;
    for (; file_read(dir, off, &e, sizeof(e)) == (int)sizeof(e); off += sizeof(e))
        if (!e.ino) break;                              // reuse a free slot, else append at the end
    memset(&e, 0, sizeof(e));
    e.ino = ino;
    strncpy(e.name, name, NAME_LEN - 1);
    return file_write(dir, off, &e, sizeof(e)) == (int)sizeof(e) ? 0 : -1;
}

static int dir_empty(uint32_t dir) {
    struct dent e;
    for (uint32_t off = 0; file_read(dir, off, &e, sizeof(e)) == (int)sizeof(e); off += sizeof(e))
        if (e.ino && strcmp(e.name, ".") != 0 && strcmp(e.name, "..") != 0) return 0;
    return 1;
}

// ---- vnodes ----------------------------------------------------------------

static vnode_t* vnode_for(uint32_t ino) {
    vnode_t* vn = &g_vn[ino];
    if (!vn->ops) {
        struct dinode di;
        if (inode_read(ino, &di) < 0) return 0;
        vn->ops = &g_ops;
        vn->fs_data = (void*)(uintptr_t)ino;
        vn->size = di.size;
        vn->is_dir = di.type == T_DIR;
    }
    return vn;
}

#define INO(vn) ((uint32_t)(uintptr_t)(vn)->fs_data)

static int bob_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) {
    return vn->is_dir ? -1 : file_read(INO(vn), off, buf, len);
}

static int bob_write(vnode_t* vn, uint32_t off, const void* buf, uint32_t len) {
    if (vn->is_dir || off > vn->size) return -1;
    int r = file_write(INO(vn), off, buf, len);
    if (r > 0 && off + (uint32_t)r > vn->size) vn->size = off + (uint32_t)r;
    return r;
}

static int bob_truncate(vnode_t* vn) {
    if (vn->is_dir) return -1;
    file_truncate(INO(vn));
    vn->size = 0;
    return 0;
}

static int bob_readdir(vnode_t* vn, uint32_t index, char* out, uint32_t max) {
    if (!vn->is_dir) return -1;
    struct dent e;
    for (uint32_t off = 0; file_read(INO(vn), off, &e, sizeof(e)) == (int)sizeof(e); off += sizeof(e)) {
        if (!e.ino || strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0) continue;
        if (index-- == 0) {
            strncpy(out, e.name, max - 1);
            out[max - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

static vnode_t* bob_lookup(vnode_t* dir, const char* name) {
    if (!dir->is_dir) return 0;
    uint32_t ino = dir_find(INO(dir), name, 0);
    return ino ? vnode_for(ino) : 0;
}

static vnode_t* bob_create(vnode_t* dir, const char* name, int is_dir) {
    if (!dir->is_dir || !name[0] || strlen(name) >= NAME_LEN || strchr(name, '/')) return 0;
    if (dir_find(INO(dir), name, 0)) return 0;

    uint32_t ino = (uint32_t)bit_alloc(g_sb.ibmap, 1, NINODES);
    if (!ino) return 0;
    struct dinode di;
    memset(&di, 0, sizeof(di));
    di.type = is_dir ? T_DIR : T_FILE;
    memset(&g_vn[ino], 0, sizeof(vnode_t));
    g_unlinked[ino] = 0;
    if (inode_write(ino, &di) < 0) { bit_free(g_sb.ibmap, ino); return 0; }

    if (is_dir && (dir_add(ino, ".", ino) < 0 || dir_add(ino, "..", INO(dir)) < 0)) goto undo;
    if (dir_add(INO(dir), name, ino) < 0) goto undo;
    return vnode_for(ino);

undo:
    file_truncate(ino);
    bit_free(g_sb.ibmap, ino);
    return 0;
}

static void free_inode(uint32_t ino) {
    file_truncate(ino);
    bit_free(g_sb.ibmap, ino);
    memset(&g_vn[ino], 0, sizeof(vnode_t));
    g_unlinked[ino] = 0;
}

static int bob_unlink(vnode_t* dir, const char* name) {
    if (!dir->is_dir || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -1;
    uint32_t at;
    uint32_t ino = dir_find(INO(dir), name, &at);
    if (!ino || ino == ROOT_INO) return -1;

    vnode_t* vn = vnode_for(ino);
    if (!vn) return -1;
    if (vn->is_dir && !dir_empty(ino)) return -1;

    struct dent blank;
    memset(&blank, 0, sizeof(blank));
    if (file_write(INO(dir), at, &blank, sizeof(blank)) != (int)sizeof(blank)) return -1;

    if (vn->refs == 0) free_inode(ino);
    else g_unlinked[ino] = 1;                          // still open: freed by put on the last close
    return 0;
}

static void bob_put(vnode_t* vn) {
    uint32_t ino = INO(vn);
    if (g_unlinked[ino] && vn->refs == 0) free_inode(ino);
}

static const vnode_ops_t g_ops = {
    .read = bob_read, .readdir = bob_readdir, .lookup = bob_lookup,
    .write = bob_write, .truncate = bob_truncate, .create = bob_create,
    .unlink = bob_unlink, .put = bob_put,
};

// ---- mount / format --------------------------------------------------------

vnode_t* bobfs_mount(void) {
    uint8_t buf[BLK];
    g_ok = 0;
    if (!ata_sectors() || blk_read(0, buf) < 0) return 0;
    struct sb* s = (struct sb*)buf;
    if (s->magic != MAGIC || s->nblocks * 2 > ata_sectors() || s->ninodes != NINODES) return 0;
    g_sb = *s;
    memset(g_vn, 0, sizeof(g_vn));
    memset(g_unlinked, 0, sizeof(g_unlinked));
    g_ok = 1;
    return vnode_for(ROOT_INO);
}

int bobfs_format(void) {
    uint32_t nblocks = ata_sectors() / 2;
    if (nblocks < 32) return -1;
    if (nblocks > 8 * BLK) nblocks = 8 * BLK;          // one bitmap block covers 8192 blocks

    memset(&g_sb, 0, sizeof(g_sb));
    g_sb.magic = MAGIC;
    g_sb.nblocks = nblocks;
    g_sb.ninodes = NINODES;
    g_sb.ibmap = 1;
    g_sb.dbmap = 2;
    g_sb.itable = 3;
    g_sb.itable_blocks = (NINODES + IPB - 1) / IPB;
    g_sb.data_start = g_sb.itable + g_sb.itable_blocks;

    uint8_t z[BLK];
    memset(z, 0, BLK);
    for (uint32_t b = 1; b < g_sb.data_start; b++) if (blk_write(b, z) < 0) return -1;

    // reserve inode 0 and the metadata blocks, then build the root directory
    uint8_t m[BLK];
    memset(m, 0, BLK);
    m[0] = 1;
    if (blk_write(g_sb.ibmap, m) < 0) return -1;
    memset(m, 0, BLK);
    for (uint32_t b = 0; b < g_sb.data_start; b++) m[b / 8] |= (uint8_t)(1u << (b % 8));
    if (blk_write(g_sb.dbmap, m) < 0) return -1;

    memset(z, 0, BLK);
    memcpy(z, &g_sb, sizeof(g_sb));
    if (blk_write(0, z) < 0) return -1;

    g_ok = 1;
    memset(g_vn, 0, sizeof(g_vn));
    uint32_t root = (uint32_t)bit_alloc(g_sb.ibmap, 1, NINODES);
    struct dinode di;
    memset(&di, 0, sizeof(di));
    di.type = T_DIR;
    if (root != ROOT_INO || inode_write(root, &di) < 0) return -1;
    if (dir_add(root, ".", root) < 0 || dir_add(root, "..", root) < 0) return -1;
    memset(g_vn, 0, sizeof(g_vn));
    return 0;
}
