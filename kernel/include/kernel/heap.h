#pragma once
#include <stddef.h>
#include <stdint.h>

void heap_init(uintptr_t heap_start, size_t heap_size);
void* kmalloc(size_t size);
void* kmalloc_aligned(size_t size, size_t align);
void  kfree(void* ptr);                 // no-op for bump
size_t heap_bytes_used(void);
size_t heap_bytes_total(void);
