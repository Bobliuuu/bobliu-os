#include <stdint.h>
#include <stdio.h>
#include <arch/i386/pmm.h>

static void print_kib(uint32_t frames) {
    printf("%u KiB", frames * 4u);
}

int cmd_mem(int argc, char** argv) {
    (void)argc; (void)argv;

    uint32_t total = pmm_total_frames();
    uint32_t free  = pmm_free_frames();
    uint32_t used  = pmm_used_frames();

    printf("frames: total=%u free=%u used=%u\n", total, free, used);
    printf("bytes : total="); print_kib(total);
    printf(" free=");         print_kib(free);
    printf(" used=");         print_kib(used);
    printf("\n");

    return 0;
}

/*
#include <stdint.h>
#include <arch/i386/pmm.h>

extern void vga_print(const char* s);
extern void vga_print_dec(uint32_t x);

static void print_kib(uint32_t frames) {
    vga_print_dec(frames * 4u);
    vga_print(" KiB");
}

int cmd_mem(int argc, char** argv) {
    (void)argc; (void)argv;

    uint32_t total = pmm_total_frames();
    uint32_t free  = pmm_free_frames();
    uint32_t used  = pmm_used_frames();

    vga_print("frames: total=");
    vga_print_dec(total);
    vga_print(" free=");
    vga_print_dec(free);
    vga_print(" used=");
    vga_print_dec(used);
    vga_print("\n");

    vga_print("bytes : total=");
    print_kib(total);
    vga_print(" free=");
    print_kib(free);
    vga_print(" used=");
    print_kib(used);
    vga_print("\n");

    return 0;
}
*/