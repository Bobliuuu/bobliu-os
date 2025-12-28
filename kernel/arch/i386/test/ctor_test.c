#include <stdint.h>

__attribute__((constructor))
static void ctor_test(void) {
    volatile uint16_t* vga = (uint16_t*)0xB8000;
    for (int i = 0; i < 80; i++) {
        vga[i] = (0x4F << 8) | 'C'; // white-on-red 'C'
    }
    //for(;;) __asm__ volatile("hlt");
}