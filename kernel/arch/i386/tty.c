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
static int cursor_hidden;
static int wrap_pending;       // last column was written: wrap happens on the next printable char
static void cursor_sync(void) {
	uint16_t pos = (uint16_t)(terminal_row * VGA_WIDTH + terminal_column);
	outb(0x3D4, 14); outb(0x3D5, pos >> 8);
	outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
	outb(0x3D4, 10); outb(0x3D5, cursor_hidden ? 0x20 : 14);   // cursor start scanline, bit 5 = off
}

// ANSI subset used by full screen programs (terminfo "bobliu"): cursor movement, erase,
// insert/delete line/char, SGR colours, cursor visibility, save/restore cursor.
static int esc_state;          // 0 plain, 1 saw ESC, 2 inside CSI
#define ESC_MAXP 8
static int esc_par[ESC_MAXP], esc_n, esc_priv;
static int sv_row, sv_col;

static int g_fg = 7, g_bg = 0, g_bold, g_inv;
static const uint8_t ansi2vga[8] = {0, 4, 2, 6, 1, 5, 3, 7};

static void update_color(void) {
	int f = g_fg + (g_bold ? 8 : 0), b = g_bg;
	terminal_color = g_inv ? (uint8_t)((b & 15) | ((f & 7) << 4)) : (uint8_t)(f | (b << 4));
}

static void sgr(int n) {
	if (n == 0) { g_fg = 7; g_bg = 0; g_bold = 0; g_inv = 0; }
	else if (n == 1) g_bold = 1;
	else if (n == 22) g_bold = 0;
	else if (n == 7) g_inv = 1;
	else if (n == 27) g_inv = 0;
	else if (n >= 30 && n <= 37) g_fg = ansi2vga[n - 30];
	else if (n == 39) g_fg = 7;
	else if (n >= 40 && n <= 47) g_bg = ansi2vga[n - 40];
	else if (n == 49) g_bg = 0;
	else if (n >= 90 && n <= 97) { g_fg = ansi2vga[n - 90] | 8; }
	else if (n >= 100 && n <= 107) g_bg = ansi2vga[n - 100];
}

static void blank_cells(size_t from, size_t to) {   // [from, to) in cell index
	for (size_t i = from; i < to; i++) terminal_buffer[i] = vga_entry(' ', terminal_color);
}

static void ansi_final(char f) {
	int a = esc_par[0], b = esc_par[1];
	int n = a > 0 ? a : 1;
	size_t cur = terminal_row * VGA_WIDTH + terminal_column;
	wrap_pending = 0;
	if (esc_priv) {
		if (esc_priv == 1 && f == 'l' && a == 25) cursor_hidden = 1;
		else if (esc_priv == 1 && f == 'h' && a == 25) cursor_hidden = 0;
		return;
	}
	switch (f) {
	case 'A': terminal_row = (size_t)n > terminal_row ? 0 : terminal_row - n; break;
	case 'B': terminal_row += n; if (terminal_row >= VGA_HEIGHT) terminal_row = VGA_HEIGHT - 1; break;
	case 'C': terminal_column += n; if (terminal_column >= VGA_WIDTH) terminal_column = VGA_WIDTH - 1; break;
	case 'D': terminal_column = (size_t)n > terminal_column ? 0 : terminal_column - n; break;
	case 'G': terminal_column = (size_t)(n - 1) >= VGA_WIDTH ? VGA_WIDTH - 1 : (size_t)(n - 1); break;
	case 'd': terminal_row = (size_t)(n - 1) >= VGA_HEIGHT ? VGA_HEIGHT - 1 : (size_t)(n - 1); break;
	case 'H': case 'f':
		terminal_row = (a > 0 ? a : 1) - 1;
		terminal_column = (b > 0 ? b : 1) - 1;
		if (terminal_row >= VGA_HEIGHT) terminal_row = VGA_HEIGHT - 1;
		if (terminal_column >= VGA_WIDTH) terminal_column = VGA_WIDTH - 1;
		break;
	case 'J':
		if (a == 0) blank_cells(cur, VGA_WIDTH * VGA_HEIGHT);
		else if (a == 1) blank_cells(0, cur + 1);
		else blank_cells(0, VGA_WIDTH * VGA_HEIGHT);   /* cursor stays, like a real terminal */
		break;
	case 'K':
		if (a == 0) blank_cells(cur, (terminal_row + 1) * VGA_WIDTH);
		else if (a == 1) blank_cells(terminal_row * VGA_WIDTH, cur + 1);
		else blank_cells(terminal_row * VGA_WIDTH, (terminal_row + 1) * VGA_WIDTH);
		break;
	case 'L': {                       // insert n lines at the cursor row
		size_t r = terminal_row, rows = VGA_HEIGHT - r;
		if ((size_t)n > rows) n = (int)rows;
		memmove(&terminal_buffer[(r + n) * VGA_WIDTH], &terminal_buffer[r * VGA_WIDTH], (rows - n) * VGA_WIDTH * 2);
		blank_cells(r * VGA_WIDTH, (r + n) * VGA_WIDTH);
		terminal_column = 0;
		break;
	}
	case 'M': {                       // delete n lines
		size_t r = terminal_row, rows = VGA_HEIGHT - r;
		if ((size_t)n > rows) n = (int)rows;
		memmove(&terminal_buffer[r * VGA_WIDTH], &terminal_buffer[(r + n) * VGA_WIDTH], (rows - n) * VGA_WIDTH * 2);
		blank_cells((VGA_HEIGHT - n) * VGA_WIDTH, VGA_HEIGHT * VGA_WIDTH);
		terminal_column = 0;
		break;
	}
	case '@': {                       // insert n blanks
		size_t room = VGA_WIDTH - terminal_column;
		if ((size_t)n > room) n = (int)room;
		memmove(&terminal_buffer[cur + n], &terminal_buffer[cur], (room - n) * 2);
		blank_cells(cur, cur + n);
		break;
	}
	case 'P': {                       // delete n chars
		size_t room = VGA_WIDTH - terminal_column;
		if ((size_t)n > room) n = (int)room;
		memmove(&terminal_buffer[cur], &terminal_buffer[cur + n], (room - n) * 2);
		blank_cells(cur + room - n, cur + room);
		break;
	}
	case 'X': {                       // erase n chars
		size_t room = VGA_WIDTH - terminal_column;
		blank_cells(cur, cur + ((size_t)n > room ? room : (size_t)n));
		break;
	}
	case 'S': { size_t r = terminal_row, c = terminal_column; for (int i = 0; i < n; i++) terminal_scroll(); terminal_row = r; terminal_column = c; break; }
	case 's': sv_row = (int)terminal_row; sv_col = (int)terminal_column; break;
	case 'u': terminal_row = (size_t)sv_row; terminal_column = (size_t)sv_col; break;
	case 'm':
		for (int i = 0; i <= esc_n && i < ESC_MAXP; i++) sgr(esc_par[i]);
		update_color();
		break;
	}
}

static void terminal_putchar_raw(char c);

// esc_state: 0 plain, 1 saw ESC, 2 inside CSI, 3 ESC + intermediate (one more byte to swallow),
// 4 inside an OSC/DCS-style string (swallowed until BEL or ESC \)
void terminal_putchar(char c) {
	unsigned char u = (unsigned char)c;
	if (esc_state == 0 && c != 27) { terminal_putchar_raw(c); cursor_sync(); return; }
	if (esc_state == 0) { esc_state = 1; return; }
	if (esc_state == 4) {
		if (c == 7) esc_state = 0;
		else if (c == 27) esc_state = 1;      // maybe ESC \ (string terminator): the '\' is eaten below
		return;
	}
	if (esc_state == 3) { esc_state = 0; return; }       // e.g. the 'B' of ESC ( B
	if (esc_state == 1) {
		if (c == '[') { esc_state = 2; esc_n = 0; esc_priv = 0; for (int i = 0; i < ESC_MAXP; i++) esc_par[i] = 0; }
		else if (c == ']' || c == 'P' || c == '_' || c == '^' || c == 'X') esc_state = 4;
		else if (u >= 0x20 && u <= 0x2F) esc_state = 3;   // ESC ( B, ESC # 8 ...
		else {
			esc_state = 0;
			if (c == '7') { sv_row = (int)terminal_row; sv_col = (int)terminal_column; }
			else if (c == '8') { terminal_row = (size_t)sv_row; terminal_column = (size_t)sv_col; wrap_pending = 0; cursor_sync(); }
			else if (c == 'c') { sgr(0); update_color(); esc_priv = 0; esc_par[0] = 2; ansi_final('J'); terminal_row = terminal_column = 0; cursor_sync(); }
			// anything else (ESC =, ESC >, ESC \ ...) is a complete sequence we do not act on
		}
		return;
	}
	// inside CSI: parameter bytes 0-9 ; and the private markers < = > ?, intermediate bytes, final byte
	if (c >= '0' && c <= '9') { if (esc_n < ESC_MAXP) esc_par[esc_n] = esc_par[esc_n] * 10 + (c - '0'); }
	else if (c == ';' || c == ':') { if (esc_n < ESC_MAXP - 1) esc_n++; }
	else if (c == '?') esc_priv = 1;
	else if (c == '<' || c == '=' || c == '>') esc_priv = 2;     // e.g. ESC[>4;2m: not an SGR, must not be run as one
	else if (u >= 0x20 && u <= 0x2F) { }                         // intermediate (ESC[ q, ESC[!p): swallow
	else if (u >= 0x40 && u <= 0x7E) { esc_state = 0; ansi_final(c); cursor_sync(); }
	else esc_state = 0;                                          // a control byte inside a sequence aborts it
}

static void terminal_putchar_raw(char c) {
	if (c == '\n') {
		wrap_pending = 0;
		terminal_column = 0;
		terminal_row++;
		if (terminal_row == VGA_HEIGHT) terminal_scroll();
		return;
	}
	if (c == '\r') {
		wrap_pending = 0;
		terminal_column = 0;
		return;
	}
	if (c == '\b') {                 // cursor left (wraps to the previous row), no erase
		if (wrap_pending) wrap_pending = 0;
		if (terminal_column > 0) terminal_column--;
		else if (terminal_row > 0) { terminal_row--; terminal_column = VGA_WIDTH - 1; }
		return;
	}
	if (c == '\t') {
		wrap_pending = 0;
		terminal_column = (terminal_column | 7) + 1;
		if (terminal_column >= VGA_WIDTH) terminal_column = VGA_WIDTH - 1;
		return;
	}
	if (c == '\a') return;
	if (wrap_pending) {
		wrap_pending = 0;
		terminal_column = 0;
		terminal_row++;
		if (terminal_row == VGA_HEIGHT) terminal_scroll();
	}
	unsigned char uc = c;
	terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
	if (terminal_column + 1 == VGA_WIDTH) wrap_pending = 1;
	else terminal_column++;
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

void terminal_backspace(void) {
    if (wrap_pending) {              // cursor sits on the last cell: erase it in place
        wrap_pending = 0;
        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        cursor_sync();
        return;
    }
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
