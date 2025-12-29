#pragma once
#include <stdint.h>

#define USER_CS 0x1B
#define USER_DS 0x23

void enter_user(uint32_t entry, uint32_t user_stack_top) __attribute__((noreturn));
