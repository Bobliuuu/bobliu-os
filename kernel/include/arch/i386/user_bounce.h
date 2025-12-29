#pragma once
#include <stdint.h>

void usermode_return_to_kernel(void);
void exec_bounce_to_kernel(void) __attribute__((noreturn));
int exec_enter_usermode(uint32_t entry, uint32_t user_stack_top);
