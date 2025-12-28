#pragma once
#include <stddef.h>
#include <stdint.h>

void initrd_init_from_module(uintptr_t start, uintptr_t end);

// list files at root (no recursion for now)
void initrd_ls(void);

// cat exact file path (e.g. "hello.txt" or "dir/note.txt")
int initrd_cat(const char* path);
