#pragma once
#include <kernel/vfs.h>

// in-memory writable filesystem (lives on the kernel heap); mount it somewhere with vfs_mount
vnode_t* ramfs_create_root(void);
