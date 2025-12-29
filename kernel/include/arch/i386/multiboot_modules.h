#pragma once
#include <stdint.h>
#include <stddef.h>
#include <arch/i386/multiboot_1.h>

typedef struct {
    uintptr_t start;
    uintptr_t end;
    const char* string;
} mb1_module_view_t;

// returns 0 on success, -1 on failure
int mb1_get_module(int index, mb1_module_view_t* out);
int mb1_find_module_by_string(const char* needle, mb1_module_view_t* out);
void multiboot1_dump_modules(multiboot_info_t* mbi);
int multiboot1_find_module(multiboot_info_t* mbi, const char* want_substr, uintptr_t* out_start, 
                            uintptr_t* out_end);