// tiny in-memory filesystem: a tree of nodes, file data in kmalloc'd buffers.
// used for /tmp. nodes stay alive while they are linked in the tree or open.
#include <kernel/ramfs.h>
#include <kernel/heap.h>
#include <string.h>
#include <stdint.h>

#define NAME_MAX_LEN 32
#define FILE_MAX     (256u * 1024u)

typedef struct rnode {
    char name[NAME_MAX_LEN];
    vnode_t vn;
    struct rnode* parent;
    struct rnode* kids;       // dirs: first child
    struct rnode* sibling;    // next entry in the same dir
    uint8_t* data;            // files
    uint32_t cap;
    int unlinked;             // removed from the tree, waiting for the last close
} rnode_t;

static const vnode_ops_t g_ops;
static rnode_t* g_root = 0;

#define RN(vn) ((rnode_t*)(vn)->fs_data)

static rnode_t* node_new(const char* name, int is_dir) {
    rnode_t* n = (rnode_t*)kmalloc(sizeof(rnode_t));
    if (!n) return 0;
    memset(n, 0, sizeof(*n));
    strncpy(n->name, name, NAME_MAX_LEN - 1);
    n->vn.ops = &g_ops;
    n->vn.fs_data = n;
    n->vn.is_dir = is_dir ? 1 : 0;
    return n;
}

static void node_free(rnode_t* n) {
    if (n->data) kfree(n->data);
    kfree(n);
}

vnode_t* ramfs_create_root(void) {
    g_root = node_new("", 1);
    return g_root ? &g_root->vn : 0;
}

static int ramfs_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) {
    rnode_t* n = RN(vn);
    if (vn->is_dir) return -1;
    if (off >= vn->size) return 0;
    if (len > vn->size - off) len = vn->size - off;
    memcpy(buf, n->data + off, len);
    return (int)len;
}

static int ramfs_write(vnode_t* vn, uint32_t off, const void* buf, uint32_t len) {
    rnode_t* n = RN(vn);
    if (vn->is_dir) return -1;
    if (off > vn->size) return -1;
    if (len > FILE_MAX || off > FILE_MAX - len) return -1;

    uint32_t end = off + len;
    if (end > n->cap) {
        uint32_t ncap = n->cap ? n->cap : 256;
        while (ncap < end) ncap *= 2;
        if (ncap > FILE_MAX) ncap = FILE_MAX;
        uint8_t* nd = (uint8_t*)kmalloc(ncap);
        if (!nd) return -1;
        if (n->data) { memcpy(nd, n->data, vn->size); kfree(n->data); }
        n->data = nd;
        n->cap = ncap;
    }
    memcpy(n->data + off, buf, len);
    if (end > vn->size) vn->size = end;
    return (int)len;
}

static int ramfs_truncate(vnode_t* vn) {
    if (vn->is_dir) return -1;
    vn->size = 0;
    return 0;
}

static int ramfs_readdir(vnode_t* vn, uint32_t index, char* out, uint32_t max) {
    if (!vn->is_dir) return -1;
    rnode_t* c = RN(vn)->kids;
    while (c && index--) c = c->sibling;
    if (!c) return 0;
    strncpy(out, c->name, max - 1);
    out[max - 1] = '\0';
    return 1;
}

static vnode_t* ramfs_lookup(vnode_t* dir, const char* name) {
    if (!dir->is_dir) return 0;
    if (strcmp(name, ".") == 0) return dir;
    rnode_t* d = RN(dir);
    if (strcmp(name, "..") == 0) return d->parent ? &d->parent->vn : dir;
    for (rnode_t* c = d->kids; c; c = c->sibling)
        if (strcmp(c->name, name) == 0) return &c->vn;
    return 0;
}

static vnode_t* ramfs_create(vnode_t* dir, const char* name, int is_dir) {
    if (!dir->is_dir || !name[0] || strlen(name) >= NAME_MAX_LEN || strchr(name, '/')) return 0;
    if (ramfs_lookup(dir, name)) return 0;
    rnode_t* d = RN(dir);
    rnode_t* n = node_new(name, is_dir);
    if (!n) return 0;
    n->parent = d;
    n->sibling = d->kids;
    d->kids = n;
    return &n->vn;
}

static int ramfs_unlink(vnode_t* dir, const char* name) {
    if (!dir->is_dir) return -1;
    rnode_t* d = RN(dir);
    for (rnode_t** pp = &d->kids; *pp; pp = &(*pp)->sibling) {
        rnode_t* c = *pp;
        if (strcmp(c->name, name) != 0) continue;
        if (c->vn.is_dir && c->kids) return -1;      // not empty
        *pp = c->sibling;
        c->unlinked = 1;
        if (c->vn.refs == 0) node_free(c);           // else the last close frees it
        return 0;
    }
    return -1;
}

static void ramfs_put(vnode_t* vn) {
    rnode_t* n = RN(vn);
    if (n->unlinked && vn->refs == 0) node_free(n);
}

static const vnode_ops_t g_ops = {
    .read = ramfs_read,
    .readdir = ramfs_readdir,
    .lookup = ramfs_lookup,
    .write = ramfs_write,
    .truncate = ramfs_truncate,
    .create = ramfs_create,
    .unlink = ramfs_unlink,
    .put = ramfs_put,
};
