#pragma once
#include <stdint.h>
#include <kernel/vfs.h>

void initrd_vfs_init(uintptr_t start, uintptr_t end);
vnode_t* initrd_vfs_root(void);
