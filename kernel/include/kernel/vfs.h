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

    // optional, for writable filesystems
    int (*write)(vnode_t* vn, uint32_t off, const void* buf, uint32_t len);   // bytes written or <0
    int (*truncate)(vnode_t* vn);                                              // size -> 0
    vnode_t* (*create)(vnode_t* dir, const char* name, int is_dir);           // new child or NULL
    int (*unlink)(vnode_t* dir, const char* name);                            // file or empty dir

    // done with a vnode that lookup/create handed out (nobody has it open any more)
    void (*put)(vnode_t* vn);
} vnode_ops_t;

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR   0x002
#define O_CREAT  0x040
#define O_TRUNC  0x200
#define O_APPEND 0x400

struct vnode {
    const vnode_ops_t* ops;
    void* fs_data;           // filesystem-specific pointer
    uint32_t size;           // for files
    uint8_t  is_dir;
    uint16_t refs;           // open files pointing here (managed by the vfs)
};

typedef struct {
    vnode_t* vn;
    uint32_t off;
    uint8_t  used;
    int      flags;    // O_* from open
    uint8_t  refs;     // dup/fork share one open file; closed when this hits 0
} file_t;

typedef struct {
    uint32_t size;
    uint8_t  is_dir;
} vfs_stat_t;

// VFS core
void vfs_init(vnode_t* root);
int  vfs_open(const char* path);                     // read only
int  vfs_open_flags(const char* path, int flags);   // O_CREAT/O_TRUNC/O_APPEND/O_WRONLY/O_RDWR
int  vfs_write(int fd, const void* buf, uint32_t len);
int  vfs_unlink(const char* path);
int  vfs_mkdir(const char* path);
// graft another filesystem under an absolute path (e.g. /tmp)
int  vfs_mount(const char* path, vnode_t* root);
int  vfs_read(int fd, void* buf, uint32_t len);
int  vfs_close(int fd);
int  vfs_dup(int fd);                              // another reference to the same open file
int  vfs_seek(int fd, int32_t off, int whence);   // 0=SET 1=CUR 2=END, returns new offset or -1
// index-th entry name of directory path: 1 = written, 0 = end, <0 error
int  vfs_readdir_path(const char* path, uint32_t index, char* name_out, uint32_t name_max);

// directory helpers
int  vfs_ls(const char* path);
int  vfs_stat(const char* path, vfs_stat_t* st);