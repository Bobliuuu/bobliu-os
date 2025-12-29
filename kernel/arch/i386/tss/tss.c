#include <stdint.h>
#include <string.h>
#include <arch/i386/tss.h>

// 32-bit TSS (only fields we care about + padding)
typedef struct __attribute__((packed)) tss_entry {
    uint32_t prev_tss;

    uint32_t esp0;
    uint32_t ss0;

    uint32_t esp1;
    uint32_t ss1;

    uint32_t esp2;
    uint32_t ss2;

    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;

    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;

    uint16_t trap;
    uint16_t iomap_base;
} tss_entry_t;

static tss_entry_t g_tss;

extern void tss_flush(uint16_t tss_sel);

// provided by gdt.c
extern void gdt_set_tss(uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

void tss_set_kernel_stack(uint32_t esp0) {
    g_tss.esp0 = esp0;
}

void tss_init(void) {
    memset(&g_tss, 0, sizeof(g_tss));

    g_tss.ss0 = 0x10;                // kernel data selector
    g_tss.iomap_base = sizeof(g_tss); // disable I/O bitmap

    // install TSS descriptor into GDT (we’ll put it at index 5)
    gdt_set_tss((uint32_t)&g_tss, sizeof(g_tss) - 1, 0x89, 0x00);
    // access 0x89 = present, ring0, type=32-bit available TSS

    // load task register with selector for GDT entry 5
    // selector = index*8 = 5*8 = 0x28
    // tss_flush();
}
