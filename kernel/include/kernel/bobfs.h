#pragma once
#include <kernel/vfs.h>

// bobfs: a small unix-style filesystem on the ata disk (inodes, bitmaps, 1 KiB blocks)
vnode_t* bobfs_mount(void);   // root vnode, or NULL if the disk has no bobfs
int      bobfs_format(void);  // wipe the disk and make an empty bobfs (0 ok)
