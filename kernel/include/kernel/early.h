#pragma once
#include <stdint.h>

void kernel_early(uint32_t multiboot_magic, void* multiboot_info_ptr);
