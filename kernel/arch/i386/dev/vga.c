#include <kernel/vga.h>

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint8_t row = 0;
static uint8_t col = 0;
static uint8_t color = 0x0F; // white on black

static void vga_newline(void) {
    col = 0;
    if (++row >= 25) row = 24;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_newline();
        return;
    }

    VGA[row * 80 + col] = ((uint16_t)color << 8) | c;

    if (++col >= 80)
        vga_newline();
}

void vga_print(const char* s) {
    while (*s) vga_putc(*s++);
}

void vga_clear(void) {
    for (int i = 0; i < 80 * 25; i++)
        VGA[i] = ((uint16_t)color << 8) | ' ';
    row = col = 0;
}

void vga_print_hex(uint32_t x) {
    const char* hex = "0123456789ABCDEF";
    vga_print("0x");
    for (int i = 28; i >= 0; i -= 4)
        vga_putc(hex[(x >> i) & 0xF]);
}

void vga_print_dec(uint32_t x) {
    char buf[11];
    int i = 0;

    if (x == 0) {
        vga_putc('0');
        return;
    }

    while (x) {
        buf[i++] = '0' + (x % 10);
        x /= 10;
    }
    while (i--)
        vga_putc(buf[i]);
}
