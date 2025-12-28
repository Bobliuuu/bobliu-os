#include <stdint.h>
#include <stdbool.h>
#include <kernel/tty.h>           /* printf */
#include <arch/i386/multiboot_1.h>

static uint32_t g_mb_magic = 0;
static multiboot_info_t* g_mbi = 0;
static const char* g_cmdline = 0;

void multiboot1_init(uint32_t magic, void* mbi_ptr) {
    g_mb_magic = magic;

    if (magic != MULTIBOOT1_MAGIC) {
        printf("BAD multiboot magic: %x\n", magic);
        // Hard fail
        // for(;;) hlt; 
        return;
    }

    g_mbi = (multiboot_info_t*)mbi_ptr;

    printf("mb magic=%x\n", magic);
    printf("mb flags=%x\n", g_mbi->flags);

    /* cmdline flag is bit 2 */
    if (g_mbi->flags & (1u << 2)) {
        g_cmdline = (const char*)(uintptr_t)g_mbi->cmdline;
        printf("cmdline: %s\n", g_cmdline);
    } else {
        g_cmdline = 0;
        printf("no cmdline flag\n");
    }
}

const char* multiboot1_cmdline(void) {
    return g_cmdline;
}

multiboot_info_t* multiboot1_info(void) {
    return g_mbi;
}

uint32_t multiboot1_magic(void) {
    return g_mb_magic;
}
