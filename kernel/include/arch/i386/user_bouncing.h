#pragma once
#include <stdint.h>

extern volatile uint32_t g_user_exited;
extern volatile uint32_t g_exec_kesp;
extern volatile uint32_t g_exec_resume_eip;
extern volatile uint32_t g_exec_kcr3;

void usermode_return_to_kernel(void);
void exec_bounce_to_kernel(void) __attribute__((noreturn));
int exec_enter_usermode(uint32_t entry, uint32_t user_stack_top);
void exec_mark_kernel_stack(void);
void exec_resumed_debug(void);