#pragma once
#include <kernel/vfs.h>

// /dev: currently just hd0 (the ata disk), if one was found
vnode_t* devfs_create_root(void);
