#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct vnode vnode_t;

typedef struct {
    // read from vnode at offset -> returns bytes read (0=EOF, <0 error)
    int (*read)(vnode_t* vn, uint32_t off, void* buf, uint32_t len);

    // list directory entries by index: returns 1 if entry written, 0 if end, <0 error
    int (*readdir)(vnode_t* vn, uint32_t index, char* name_out, uint32_t name_max);

    // lookup child "name" under a directory vnode -> returns vnode* or NULL
    vnode_t* (*lookup)(vnode_t* dir, const char* name);
} vnode_ops_t;

struct vnode {
    const vnode_ops_t* ops;
    void* fs_data;           // filesystem-specific pointer
    uint32_t size;           // for files
    uint8_t  is_dir;
};

typedef struct {
    vnode_t* vn;
    uint32_t off;
    uint8_t  used;
} file_t;

typedef struct {
    uint32_t size;
    uint8_t  is_dir;
} vfs_stat_t;

// VFS core
void vfs_init(vnode_t* root);
int  vfs_open(const char* path);
int  vfs_read(int fd, void* buf, uint32_t len);
int  vfs_close(int fd);

// directory helpers
int  vfs_ls(const char* path);
int  vfs_stat(const char* path, vfs_stat_t* st);