#pragma once
#include <kernel/vfs.h>

// make a pipe: *rd / *wr are vnodes for the two ends (open them with vfs_open_vnode)
int pipe_create(vnode_t** rd, vnode_t** wr);
