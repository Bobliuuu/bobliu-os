#pragma once
#include <stdint.h>

void vga_clear(void);
void vga_putc(char c);
void vga_print(const char* s);
void vga_print_hex(uint32_t x);
void vga_print_dec(uint32_t x);
