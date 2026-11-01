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
#include <kernel/ramfs.h>
#include <kernel/devfs.h>
#include <kernel/bobfs.h>
#include <arch/i386/ata.h>
#include <arch/i386/paging.h>
#include <arch/i386/gdt.h>
#include <arch/i386/tss.h>
#include <arch/i386/idt.h>
#include <kernel/proc.h>

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
	proc_init();

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
		vfs_mount("/tmp", ramfs_create_root());
		uint32_t nsec = ata_init();
		if (nsec) printf("ata: hd0 %u sectors (%u KiB)\n", nsec, nsec / 2);
		else printf("ata: no disk\n");
		vfs_mount("/dev", devfs_create_root());
		if (nsec) {
			vnode_t* droot = bobfs_mount();
			if (droot) { vfs_mount("/disk", droot); printf("disk: bobfs mounted on /disk\n"); }
			else printf("disk: no filesystem (run mkfs in the kernel shell)\n");
		}
	} else {
		printf("no initrd module\n");
	}

	// Test userspace with ring check
	// test_ring3_int80();

    bool enable_shell = true;
    if (cmdline_has(boot_cmdline, "noshell")) {
		enable_shell = false;
	}

	keyboard_enable_shell(enable_shell);

	// init=/path boots straight into a user program (the user shell); when it
	// exits we fall back to the kernel shell
	const char* init = cmdline_has(boot_cmdline, "init=") ? strstr(boot_cmdline, "init=") + 5 : NULL;
	if (init && enable_shell) {
		char path[64];
		size_t n = 0;
		while (init[n] && init[n] != ' ' && n < sizeof(path) - 1) { path[n] = init[n]; n++; }
		path[n] = '\0';
		char* args[1] = { path };
		int pid = proc_spawn(path, args, 1);
		if (pid > 0) {
			proc_wait(pid, NULL);
			printf("[init exited]\n");
		} else {
			printf("init: cannot run %s\n", path);
		}
	}

    if (enable_shell) {
        shell_init();
    }

    // kernel context (pid 0): run shell lines queued by the keyboard irq
    for (;;) {
        shell_poll();
        proc_reap_orphans();
        __asm__ volatile("sti; hlt");
    }

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