#include <stdio.h>

#include <kernel/tty.h>
#include <arch/i386/idt.h> 
#include <kernel/shell.h>

void interrupts_init(void);

void kernel_main(void) {
    terminal_initialize();

    printf("Bobliu's kernel\n");
    interrupts_init();
	shell_init();

    for(;;) __asm__ volatile("hlt");

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