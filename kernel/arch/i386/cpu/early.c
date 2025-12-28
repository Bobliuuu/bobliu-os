#include <stdint.h>
#include <kernel/tty.h>

static inline void vga_put_at(int pos, char ch, uint8_t color) {
    volatile uint16_t* vga = (uint16_t*)0xB8000;
    vga[pos] = ((uint16_t)color << 8) | (uint16_t)ch;
}

void kernel_early(uint32_t magic, void* mbi) {
    (void)magic; (void)mbi;

    // Write "E" at top-left in bright white on red
    vga_put_at(0, 'E', 0x4F);

    // Set SSP guard here (no printf)
    extern uintptr_t __stack_chk_guard;
    __stack_chk_guard = 0xBAADF00D;

    terminal_initialize();
}