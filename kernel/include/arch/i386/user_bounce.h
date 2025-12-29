#pragma once
void usermode_return_to_kernel(void);
void exec_bounce_to_kernel(void) __attribute__((noreturn));