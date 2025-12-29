#pragma once
#include <stdint.h>

void gdt_init(void);

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS   0x1B
#define USER_DS   0x23
