#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <kernel/tty.h>
#include <kernel/shell.h>
#include <arch/i386/keyboard.h>
#include <arch/i386/ssp.h>
#include <arch/i386/pmm.h>
#include <arch/i386/multiboot_1.h>

void interrupts_init(void);
// void ssp_test_run(void);

static const char* boot_cmdline = NULL;

static bool cmdline_has(const char* cmdline, const char* flag) {
    return cmdline && strstr(cmdline, flag) != NULL;
}

#include <stdint.h>

/*
__attribute__((constructor))
static void ctor_ping(void) {
    volatile uint16_t* vga = (uint16_t*)0xB8000;
    vga[1] = (0x2F << 8) | 'C'; // green C in second cell
}
*/

void kernel_main(uint32_t multiboot_magic, void* multiboot_info_ptr) {
    //terminal_initialize();

    printf("Bobliu's kernel\n");
	// ssp_init();
	// ssp_test_run();

	/* All multiboot checks moved inside multiboot_1.c */
    multiboot1_init(multiboot_magic, multiboot_info_ptr);
    boot_cmdline = multiboot1_cmdline();

	interrupts_init();

    pmm_init(multiboot_magic, (uintptr_t)multiboot_info_ptr);

    /* TEST: mem manager */
    uintptr_t a = pmm_alloc_frame();
	uintptr_t b = pmm_alloc_frame();
	printf("alloc a=%x b=%x\n", (uint32_t)a, (uint32_t)b);

	if (a) pmm_free_frame(a);
	if (b) pmm_free_frame(b);

    bool enable_shell = true;
    if (cmdline_has(boot_cmdline, "noshell")) {
		enable_shell = false;
	}

    if (enable_shell) {
        shell_init();
    }

	keyboard_enable_shell(enable_shell);

    for(;;) __asm__ volatile("hlt");

	/* TEST FOR ERROR */
	//volatile int x = 1 / 0;
	/*
	for (;;) {
        static uint32_t last = 0;
        uint32_t t = timer_ticks();
        if (t != last) {
            last = t;
            if ((t % 100) == 0) printf("ticks=%llu\n", t);
        }
        __asm__ volatile("hlt");
    }
	*/
}