#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/tty.h>

#include <arch/i386/portio.h>
#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

static void terminal_scroll(void) {
    // Move rows 1..24 up to 0..23
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(y - 1) * VGA_WIDTH + x] =
                terminal_buffer[y * VGA_WIDTH + x];
        }
    }

    // Clear last row
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', terminal_color);
    }

    terminal_row = VGA_HEIGHT - 1;
    terminal_column = 0;
}

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

// hardware cursor follows the text position
static void cursor_sync(void) {
	uint16_t pos = (uint16_t)(terminal_row * VGA_WIDTH + terminal_column);
	outb(0x3D4, 14); outb(0x3D5, pos >> 8);
	outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
}

// tiny ANSI subset: ESC[2J clear, ESC[r;cH / ESC[H move (1-based), ESC[K clear to eol,
// ESC[7m inverse, ESC[0m / ESC[m normal
static int esc_state;          // 0 plain, 1 saw ESC, 2 inside CSI
static int esc_par[2], esc_n;

static void ansi_final(char f) {
	int a = esc_par[0], b = esc_par[1];
	switch (f) {
	case 'H': case 'f':
		terminal_row = (a > 0 ? a : 1) - 1;
		terminal_column = (b > 0 ? b : 1) - 1;
		if (terminal_row >= VGA_HEIGHT) terminal_row = VGA_HEIGHT - 1;
		if (terminal_column >= VGA_WIDTH) terminal_column = VGA_WIDTH - 1;
		break;
	case 'J':
		for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
			terminal_buffer[i] = vga_entry(' ', terminal_color);
		terminal_row = terminal_column = 0;
		break;
	case 'K': terminal_clear_eol(); break;
	case 'm':
		terminal_color = (a == 7) ? 0x70 : vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
		break;
	}
}

static void terminal_putchar_raw(char c);

void terminal_putchar(char c) {
	if (esc_state == 0 && c != 27) { terminal_putchar_raw(c); cursor_sync(); return; }
	if (esc_state == 0) { esc_state = 1; return; }
	if (esc_state == 1) {
		if (c == '[') { esc_state = 2; esc_n = 0; esc_par[0] = esc_par[1] = 0; }
		else esc_state = 0;
		return;
	}
	if (c >= '0' && c <= '9') { if (esc_n < 2) esc_par[esc_n] = esc_par[esc_n] * 10 + (c - '0'); }
	else if (c == ';') { if (esc_n < 1) esc_n++; }
	else { esc_state = 0; ansi_final(c); cursor_sync(); }
}

static void terminal_putchar_raw(char c) {
	if (c == '\n') {
		terminal_column = 0;
		terminal_row++;
		if (terminal_row == VGA_HEIGHT) terminal_scroll();
		return;
	}
	if (c == '\r') {
		terminal_column = 0;
		return;
	}
	unsigned char uc = c;
	terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		terminal_row++;
		if (terminal_row == VGA_HEIGHT) terminal_scroll();
	}
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

void terminal_backspace(void) {
    if (terminal_column == 0 && terminal_row == 0) return;

    if (terminal_column == 0) {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
    } else {
        terminal_column--;
    }

    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
}

void terminal_clear_eol(void) {
    for (size_t x = terminal_column; x < VGA_WIDTH; x++) {
        terminal_putentryat(' ', terminal_color, x, terminal_row);
    }
}
