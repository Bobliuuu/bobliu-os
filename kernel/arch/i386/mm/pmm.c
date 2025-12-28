#include <stdint.h>
#include <stddef.h>
#include <arch/i386/multiboot_1.h>
#include <arch/i386/pmm.h>
#include <stdio.h>

/* you likely already have these */
extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);
extern void vga_print_dec(uint32_t x);

/* panic helper: prints + halts */
static void panic_vga(const char* msg) {
    vga_print("PANIC: ");
    vga_print(msg);
    vga_print("\n");
    asm volatile("cli");
    for (;;) asm volatile("hlt");
}

/* linker symbol: set this in linker script */
extern uint8_t __kernel_end;

/* 4KiB frames */
#define FRAME_SIZE 4096u
#define ALIGN_UP(x,a)   (((x) + ((a)-1)) & ~((a)-1))
#define ALIGN_DOWN(x,a) ((x) & ~((a)-1))

static uint8_t*  g_bitmap = 0;     /* bitmap lives in kernel .bss/.data region after end */
static uint32_t  g_total_frames = 0;
static uint32_t  g_free_frames  = 0;

static inline void bit_set(uint32_t idx) {
    g_bitmap[idx >> 3] |=  (uint8_t)(1u << (idx & 7));
}
static inline void bit_clear(uint32_t idx) {
    g_bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7));
}
static inline int bit_test(uint32_t idx) {
    return (g_bitmap[idx >> 3] >> (idx & 7)) & 1u;
}

/* mark [paddr, paddr+len) as used/free, paddr/len are physical */
static void mark_used_range(uintptr_t paddr, uintptr_t len) {
    uintptr_t start = ALIGN_DOWN(paddr, FRAME_SIZE);
    uintptr_t end   = ALIGN_UP(paddr + len, FRAME_SIZE);
    for (uintptr_t a = start; a < end; a += FRAME_SIZE) {
        uint32_t f = (uint32_t)(a / FRAME_SIZE);
        if (f < g_total_frames && !bit_test(f)) {
            bit_set(f);
            if (g_free_frames) g_free_frames--;
        }
    }
}

static void mark_free_range(uintptr_t paddr, uintptr_t len) {
    uintptr_t start = ALIGN_UP(paddr, FRAME_SIZE);
    uintptr_t end   = ALIGN_DOWN(paddr + len, FRAME_SIZE);
    for (uintptr_t a = start; a < end; a += FRAME_SIZE) {
        uint32_t f = (uint32_t)(a / FRAME_SIZE);
        if (f < g_total_frames && bit_test(f)) {
            bit_clear(f);
            g_free_frames++;
        }
    }
}

/* find maximum address from mmap; fallback to mem_upper if needed */
static uintptr_t detect_max_phys(multiboot_info_t* mbi) {
    if (!(mbi->flags & MULTIBOOT1_INFO_MMAP)) {
        if (mbi->flags & MULTIBOOT1_INFO_MEM) {
            return 0x100000u + (uintptr_t)mbi->mem_upper * 1024u;
        }
        panic_vga("No mmap and no mem_upper");
        return 0;
    }

    uint64_t max_end = 0;

    uintptr_t cur = (uintptr_t)mbi->mmap_addr;
    uintptr_t end = cur + (uintptr_t)mbi->mmap_length;

    while (cur < end) {
        multiboot_mmap_entry_t* e = (multiboot_mmap_entry_t*)cur;

        /* only consider AVAILABLE RAM; avoids weird high reserved holes */
        if (e->type == 1) {
            uint64_t region_end = e->addr + e->len;
            if (region_end > max_end) {
                max_end = region_end;
            }
        }

        cur += (uintptr_t)e->size + 4u;
    }

    /* clamp to 32-bit phys (you are i686) */
    if (max_end > 0xFFFFFFFFull) {
        max_end = 0xFFFFFFFFull;
    }

    return (uintptr_t)max_end;
}

void pmm_init(uint32_t multiboot_magic, uintptr_t multiboot_info_phys) {
    if (multiboot_magic != MULTIBOOT1_MAGIC) {
        panic_vga("Bad multiboot magic");
    }

    multiboot_info_t* mbi = (multiboot_info_t*)multiboot_info_phys;

    printf("mmap_len=%x mmap_addr=%x\n", mbi->mmap_length, mbi->mmap_addr);

    /* we strongly prefer mmap */
    if (!(mbi->flags & MULTIBOOT1_INFO_MMAP)) {
        panic_vga("Multiboot missing mmap (flag 1<<6 not set)");
    }

    uintptr_t max_phys = detect_max_phys(mbi);
    g_total_frames = (uint32_t)(ALIGN_UP(max_phys, FRAME_SIZE) / FRAME_SIZE);

    /* bitmap bytes: 1 bit per frame */
    uint32_t bitmap_bytes = (g_total_frames + 7u) / 8u;

    /* place bitmap after kernel end (physical identity-mapped right now) */
    uintptr_t bitmap_phys = (uintptr_t)ALIGN_UP((uintptr_t)&__kernel_end, 16);
    g_bitmap = (uint8_t*)bitmap_phys;

    /* initialize bitmap: 1 = used by default */
    for (uint32_t i = 0; i < bitmap_bytes; i++) g_bitmap[i] = 0xFF;

    g_free_frames = 0;

    /* free all available RAM regions (type=1) */
    uintptr_t cur = (uintptr_t)mbi->mmap_addr;
    uintptr_t end = cur + (uintptr_t)mbi->mmap_length;
    while (cur < end) {
        multiboot_mmap_entry_t* e = (multiboot_mmap_entry_t*)cur;
        if (e->type == 1) {
            /* free it */
            mark_free_range((uintptr_t)e->addr, (uintptr_t)e->len);
        }
        cur += (uintptr_t)e->size + sizeof(e->size);
    }

    /* now re-mark critical regions as used */

    /* 1) never give out frame 0 and the very low area (BIOS stuff).
       Keeping first 1MB used is common in early kernels. */
    mark_used_range(0, 0x100000u);

    /* 2) kernel image: [kernel_start, kernel_end)
       If you have __kernel_start symbol, use it; otherwise assume kernel is loaded low and
       just protect up to __kernel_end. */
    extern uint8_t __kernel_start;
    mark_used_range((uintptr_t)&__kernel_start, (uintptr_t)(&__kernel_end - &__kernel_start));

    /* 3) bitmap storage itself */
    mark_used_range(bitmap_phys, bitmap_bytes);

    /* 4) multiboot modules (e.g., initrd later) */
    if (mbi->flags & MULTIBOOT1_INFO_MODS) {
        multiboot_module_t* mods = (multiboot_module_t*)(uintptr_t)mbi->mods_addr;
        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            uintptr_t s = (uintptr_t)mods[i].mod_start;
            uintptr_t l = (uintptr_t)(mods[i].mod_end - mods[i].mod_start);
            mark_used_range(s, l);
        }
    }

    /* sanity: if bitmap overlaps non-free area badly, you'll notice soon via mem */
    printf("max_phys=%x total_frames=%u\n", (uint32_t)max_phys, pmm_total_frames());
}

uintptr_t pmm_alloc_frame(void) {
    /* simple first-fit scan */
    for (uint32_t f = 0; f < g_total_frames; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            if (g_free_frames) g_free_frames--;
            return (uintptr_t)f * FRAME_SIZE;
        }
    }
    return 0; /* OOM */
}

void pmm_free_frame(uintptr_t paddr) {
    if ((paddr & (FRAME_SIZE - 1)) != 0) {
        panic_vga("pmm_free_frame: not 4KiB aligned");
    }
    uint32_t f = (uint32_t)(paddr / FRAME_SIZE);
    if (f >= g_total_frames) {
        panic_vga("pmm_free_frame: out of range");
    }
    if (!bit_test(f)) {
        panic_vga("pmm_free_frame: double free");
    }
    bit_clear(f);
    g_free_frames++;
}

uint32_t pmm_total_frames(void) { return g_total_frames; }
uint32_t pmm_free_frames(void)  { return g_free_frames; }
uint32_t pmm_used_frames(void)  { return g_total_frames - g_free_frames; }
