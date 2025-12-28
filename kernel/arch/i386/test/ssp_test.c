#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Weak test (works for -all only)
/*
__attribute__((constructor))
static void ssp_ctor_test(void) {
    printf("SSP ctor test...\n");
    // Cause a buf overflow
    char buf[8];
    strcpy(buf, "THIS WILL OVERFLOW");
    printf("SSP test failed...\n");
}
*/

// Strong test
__attribute__((constructor))
static void ssp_ctor_test(void) {
    // PROOF ctor runs: write "S" at top-left+2
    volatile uint16_t* vga = (uint16_t*)0xB8000;
    vga[2] = (0x4F << 8) | 'S';

    printf("[ssp] running test\n");

    
    volatile char buf[8];
    for (int i = 0; i < 64; i++) buf[i] = 'A';
    
    printf("ERROR: SSP did not trigger\n");
}