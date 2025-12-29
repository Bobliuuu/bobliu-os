#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <kernel/tty.h>
#include <kernel/shell.h>
#include <arch/i386/keyboard.h>
#include <arch/i386/ssp.h>
#include <arch/i386/pmm.h>
#include <arch/i386/multiboot_1.h>
#include <kernel/heap.h>
#include <arch/i386/multiboot_modules.h>
#include <kernel/initrd.h>
#include <kernel/vfs.h>
#include <kernel/initrd_vfs.h>
#include <arch/i386/paging.h>
#include <arch/i386/gdt.h>
#include <arch/i386/tss.h>
#include <arch/i386/idt.h>

void interrupts_init(void);
// void ssp_test_run(void);

static const char* boot_cmdline = NULL;

static uint8_t kstack[4096] __attribute__((aligned(16)));

static bool cmdline_has(const char* cmdline, const char* flag) {
    return cmdline && strstr(cmdline, flag) != NULL;
}

__attribute__((noreturn))
static void user_code(void) {
    // syscall: eax=1, bl='A'
    asm volatile(
        "mov $1, %eax\n"
        "mov $'A', %ebx\n"
        "int $0x80\n"
        "hlt\n"
    );
    for (;;) {}
}

//extern void test_ring3_int80(void);

/*
__attribute__((constructor))
static void ctor_ping(void) {
    volatile uint16_t* vga = (uint16_t*)0xB8000;
    vga[1] = (0x2F << 8) | 'C'; // green C in second cell
}
*/

void kernel_main(uint32_t multiboot_magic, void* multiboot_info_ptr) {
	// TERMINAL 
    //terminal_initialize();

    printf("Bobliu's kernel\n");

	// SSP
	// ssp_init();
	// ssp_test_run();

	// MULTIBOOT 
    multiboot1_init(multiboot_magic, multiboot_info_ptr);
    boot_cmdline = multiboot1_cmdline();

	// INTERRUPTS
	interrupts_init();

	// PMM
    pmm_init(multiboot_magic, (uintptr_t)multiboot_info_ptr);

    // TEST: mem manager 
    uintptr_t a = pmm_alloc_frame();
	uintptr_t b = pmm_alloc_frame();
	printf("alloc a=%x b=%x\n", (uint32_t)a, (uint32_t)b);

	// PAGING
	paging_init_identity();
	// Test page fault -> core dump
	/*
	volatile uint32_t* bad = (uint32_t*)0xDEADBEEF;
	(void)*bad;
	*/

	heap_init(0x00800000u, 0x00400000u);

	if (a) pmm_free_frame(a);
	if (b) pmm_free_frame(b);

	// Init GDT, TSS, IDT
	gdt_init();
	tss_init(); 
	tss_set_kernel_stack((uint32_t)(kstack + sizeof(kstack)));
	printf("[BOOT] before tss_flush\n");
    tss_flush();
    printf("[BOOT] after tss_flush\n");
	idt_init();

	// DEBUG HEXDUMP OF PMM
	multiboot_info_t* mbi = multiboot1_info();
	multiboot1_dump_modules(mbi);

	// Set VFS and initrd
	uintptr_t initrd_start=0, initrd_end=0;
	mb1_module_view_t mod;
	if (mb1_find_module_by_string("initrd", &mod) == 0) {
		initrd_vfs_init(mod.start, mod.end);
		vfs_init(initrd_vfs_root());
	} else {
		printf("no initrd module\n");
	}

	// Test userspace with ring check
	// test_ring3_int80();

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