#include <kernel/vfs.h>
#include <string.h>
#include <stdio.h>

#define MAX_FD 256

static vnode_t* g_root = 0;
static file_t g_fds[MAX_FD];

void vfs_init(vnode_t* root) {
    g_root = root;
    for (int i = 0; i < MAX_FD; i++) g_fds[i].used = 0;
}

#define MAX_MOUNTS 4
static struct { char path[32]; vnode_t* root; } g_mounts[MAX_MOUNTS];
static int g_nmounts = 0;

int vfs_mount(const char* path, vnode_t* root) {
    if (g_nmounts >= MAX_MOUNTS || !path || path[0] != '/' || strlen(path) >= sizeof(g_mounts[0].path)) return -1;
    strcpy(g_mounts[g_nmounts].path, path);
    g_mounts[g_nmounts].root = root;
    g_nmounts++;
    return 0;
}

int vfs_is_mounted(const char* path) {
    for (int i = 0; i < g_nmounts; i++) if (strcmp(g_mounts[i].path, path) == 0) return 1;
    return 0;
}

// done with a vnode that isn't held open by anybody
static void vput(vnode_t* vn) {
    if (vn && vn->refs == 0 && vn->ops && vn->ops->put) vn->ops->put(vn);
}

// if path sits under a mount point, return that fs root and move *pp past the mount path
static vnode_t* mount_for(const char** pp) {
    const char* path = *pp;
    for (int i = 0; i < g_nmounts; i++) {
        size_t n = strlen(g_mounts[i].path);
        if (strncmp(path, g_mounts[i].path, n) == 0 && (path[n] == '\0' || path[n] == '/')) {
            *pp = path + n;
            return g_mounts[i].root;
        }
    }
    return 0;
}

// walk path; if want_parent, stop before the last component and copy it to last[]
static vnode_t* walk(const char* path, int want_parent, char* last, size_t last_max) {
    if (!g_root || !path || path[0] != '/') return 0;

    const char* p = path;
    vnode_t* cur = mount_for(&p);
    int fresh = 0;                 // cur came from lookup (not a root), so we own it
    if (!cur) { cur = g_root; p = path; }
    char part[128];

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        size_t n = 0;
        while (p[n] && p[n] != '/' && n + 1 < sizeof(part)) n++;
        memcpy(part, p, n);
        part[n] = '\0';
        p += n;
        while (*p == '/') p++;

        if (want_parent && !*p) {
            if (last) { strncpy(last, part, last_max - 1); last[last_max - 1] = '\0'; }
            return cur;
        }

        if (!cur->is_dir || !cur->ops || !cur->ops->lookup) { if (fresh) vput(cur); return 0; }

        vnode_t* nxt = cur->ops->lookup(cur, part);
        if (nxt != cur && fresh) vput(cur);
        if (!nxt) return 0;
        cur = nxt;
        fresh = 1;
    }

    if (want_parent) { if (fresh) vput(cur); return 0; }   // path had no last component
    return cur;
}

static vnode_t* vfs_resolve(const char* path) { return walk(path, 0, 0, 0); }

// put an already-resolved vnode into the open file table (takes over the caller's reference)
int vfs_open_vnode(vnode_t* vn, int flags) {
    for (int fd = 0; fd < MAX_FD; fd++) {
        if (!g_fds[fd].used) {
            g_fds[fd].used = 1;
            g_fds[fd].vn = vn;
            g_fds[fd].off = 0;
            g_fds[fd].refs = 1;
            g_fds[fd].flags = flags;
            vn->refs++;
            return fd;
        }
    }
    return -1;
}

int vfs_open_flags(const char* path, int flags) {
    vnode_t* vn = vfs_resolve(path);
    int acc = flags & 3;

    if (!vn && (flags & O_CREAT)) {
        char name[64];
        vnode_t* dir = walk(path, 1, name, sizeof(name));
        if (!dir || !dir->is_dir || !dir->ops || !dir->ops->create) { vput(dir); return -1; }
        vn = dir->ops->create(dir, name, 0);
        vput(dir);
    }
    if (!vn || vn->is_dir) { vput(vn); return -1; }
    if (acc != O_RDONLY && !(vn->ops && vn->ops->write)) { vput(vn); return -1; }
    if (acc != O_WRONLY && !(vn->ops && vn->ops->read)) { vput(vn); return -1; }
    if ((flags & O_TRUNC) && acc != O_RDONLY && vn->ops->truncate) vn->ops->truncate(vn);

    int fd = vfs_open_vnode(vn, flags);
    if (fd < 0) vput(vn);
    return fd;
}

int vfs_open(const char* path) { return vfs_open_flags(path, O_RDONLY); }

int vfs_write(int fd, const void* buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FD || !g_fds[fd].used) return -1;
    file_t* f = &g_fds[fd];
    if ((f->flags & 3) == O_RDONLY || !f->vn->ops->write) return -1;
    if (f->flags & O_APPEND) f->off = f->vn->size;
    int r = f->vn->ops->write(f->vn, f->off, buf, len);
    if (r > 0) f->off += (uint32_t)r;
    return r;
}

int vfs_unlink(const char* path) {
    char name[64];
    vnode_t* dir = walk(path, 1, name, sizeof(name));
    int r = -1;
    if (dir && dir->is_dir && dir->ops && dir->ops->unlink) r = dir->ops->unlink(dir, name);
    vput(dir);
    return r;
}

int vfs_mkdir(const char* path) {
    char name[64];
    vnode_t* existing = vfs_resolve(path);
    if (existing) { vput(existing); return -1; }
    vnode_t* dir = walk(path, 1, name, sizeof(name));
    if (!dir || !dir->is_dir || !dir->ops || !dir->ops->create) { vput(dir); return -1; }
    vnode_t* nd = dir->ops->create(dir, name, 1);
    vput(nd);
    vput(dir);
    return nd ? 0 : -1;
}

int vfs_read(int fd, void* buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    if (!g_fds[fd].used) return -1;

    file_t* f = &g_fds[fd];
    if ((f->flags & 3) == O_WRONLY) return -1;
    int r = f->vn->ops->read(f->vn, f->off, buf, len);
    if (r > 0) f->off += (uint32_t)r;
    return r;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FD || !g_fds[fd].used) return -1;
    if (--g_fds[fd].refs == 0) {
        vnode_t* vn = g_fds[fd].vn;
        g_fds[fd].used = 0;
        vn->refs--;
        vput(vn);
    }
    return 0;
}

int vfs_dup(int fd) {
    if (fd < 0 || fd >= MAX_FD || !g_fds[fd].used) return -1;
    g_fds[fd].refs++;
    return fd;
}

int vfs_seek(int fd, int32_t off, int whence) {
    if (fd < 0 || fd >= MAX_FD || !g_fds[fd].used) return -1;
    file_t* f = &g_fds[fd];
    int64_t base = whence == 0 ? 0 : whence == 1 ? (int64_t)f->off : whence == 2 ? (int64_t)f->vn->size : -1;
    if (base < 0) return -1;
    int64_t n = base + off;
    if (n < 0 || n > (int64_t)f->vn->size) return -1;
    f->off = (uint32_t)n;
    return (int)n;
}

int vfs_readdir_path(const char* path, uint32_t index, char* name_out, uint32_t name_max) {
    vnode_t* vn = vfs_resolve(path);
    if (!vn || !vn->is_dir || !vn->ops || !vn->ops->readdir) { vput(vn); return -1; }
    int r = vn->ops->readdir(vn, index, name_out, name_max);
    vput(vn);
    return r;
}

int vfs_ls(const char* path) {
    vnode_t* vn = vfs_resolve(path ? path : "/");
    if (!vn || !vn->is_dir || !vn->ops || !vn->ops->readdir) { vput(vn); return -1; }

    char name[128];
    for (uint32_t i = 0;; i++) {
        int r = vn->ops->readdir(vn, i, name, sizeof(name));
        if (r == 0) break;
        if (r < 0) { vput(vn); return -1; }
        printf("%s\n", name);
    }
    vput(vn);
    return 0;
}

int vfs_stat(const char* path, vfs_stat_t* st) {
    if (!st) return -1;

    vnode_t* vn = vfs_resolve(path);
    if (!vn) return -1;

    st->size   = vn->size;
    st->is_dir = vn->is_dir ? 1 : 0;
    vput(vn);
    return 0;
}

// readiness of an open file for poll(); plain files are always ready
int vfs_poll(int fd, int events) {
    if (fd < 0 || fd >= MAX_FD || !g_fds[fd].used) return 0x20;   // POLLNVAL
    vnode_t* vn = g_fds[fd].vn;
    if (!vn->ops->poll) return events & 5;
    return vn->ops->poll(vn, events);
}
