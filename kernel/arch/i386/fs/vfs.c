#include <kernel/vfs.h>
#include <string.h>
#include <stdio.h>

#define MAX_FD 32

static vnode_t* g_root = 0;
static file_t g_fds[MAX_FD];

void vfs_init(vnode_t* root) {
    g_root = root;
    for (int i = 0; i < MAX_FD; i++) g_fds[i].used = 0;
}

static vnode_t* vfs_resolve(const char* path) {
    if (!g_root || !path || path[0] != '/') return 0;

    const char* p = path + 1; // skip leading '/'
    vnode_t* cur = g_root;
    char part[128];

    while (*p) {
        // skip repeated slashes
        while (*p == '/') p++;
        if (!*p) break;

        // extract next path component
        size_t n = 0;
        while (p[n] && p[n] != '/' && n + 1 < sizeof(part)) n++;
        memcpy(part, p, n);
        part[n] = '\0';
        p += n;

        if (!cur->is_dir || !cur->ops || !cur->ops->lookup) return 0;

        cur = cur->ops->lookup(cur, part);
        if (!cur) return 0;
    }

    return cur;
}

int vfs_open(const char* path) {
    vnode_t* vn = vfs_resolve(path);
    if (!vn || vn->is_dir) return -1;
    if (!vn->ops || !vn->ops->read) return -1;

    for (int fd = 0; fd < MAX_FD; fd++) {
        if (!g_fds[fd].used) {
            g_fds[fd].used = 1;
            g_fds[fd].vn = vn;
            g_fds[fd].off = 0;
            return fd;
        }
    }
    return -1;
}

int vfs_read(int fd, void* buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    if (!g_fds[fd].used) return -1;

    file_t* f = &g_fds[fd];
    int r = f->vn->ops->read(f->vn, f->off, buf, len);
    if (r > 0) f->off += (uint32_t)r;
    return r;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    g_fds[fd].used = 0;
    return 0;
}

int vfs_ls(const char* path) {
    vnode_t* vn = vfs_resolve(path ? path : "/");
    if (!vn || !vn->is_dir || !vn->ops || !vn->ops->readdir) return -1;

    char name[128];
    for (uint32_t i = 0;; i++) {
        int r = vn->ops->readdir(vn, i, name, sizeof(name));
        if (r == 0) break;
        if (r < 0) return -1;
        printf("%s\n", name);
    }
    return 0;
}

int vfs_stat(const char* path, vfs_stat_t* st) {
    if (!st) return -1;

    vnode_t* vn = vfs_resolve(path);
    if (!vn) return -1;

    st->size   = vn->size;
    st->is_dir = vn->is_dir ? 1 : 0;
    return 0;
}
