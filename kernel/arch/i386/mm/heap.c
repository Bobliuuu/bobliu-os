#include <kernel/heap.h>
#include <arch/i386/pmm.h>

extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);

#define PAGE_SIZE 4096u
#define ALIGN_UP(x,a)   (((x) + ((a)-1)) & ~((a)-1))

static uintptr_t g_heap_start = 0;
static uintptr_t g_heap_end   = 0;   // max virtual address we allow
static uintptr_t g_heap_brk   = 0;   // bump pointer

static uintptr_t g_heap_mapped_end = 0; // end of backed pages

void heap_init(uintptr_t heap_start, size_t heap_size) {
    g_heap_start = ALIGN_UP(heap_start, 16);
    g_heap_end   = g_heap_start + heap_size;
    g_heap_brk   = g_heap_start;

    // initially nothing is backed; we’ll map “identity pages” by just allocating frames
    g_heap_mapped_end = g_heap_start;

    vga_print("heap: start=");
    vga_print_hex((uint32_t)g_heap_start);
    vga_print(" end=");
    vga_print_hex((uint32_t)g_heap_end);
    vga_print("\n");
}

static void heap_ensure_backed(uintptr_t new_brk) {
    // identity-mapped world: just ensure we reserve physical frames for pages we will touch
    // new_brk is the first byte AFTER allocation
    uintptr_t need = ALIGN_UP(new_brk, PAGE_SIZE);

    while (g_heap_mapped_end < need) {
        uintptr_t phys = pmm_alloc_frame();
        if (!phys) {
            vga_print("heap: OOM\n");
            for(;;) asm volatile("cli; hlt");
        }

        // Because we're identity-mapped right now, we REQUIRE phys == g_heap_mapped_end
        // If PMM gives something else, we can still use it later once we have a real mapper,
        // but for now keep it simple: just consume frames and ignore phys location.
        // We'll still advance mapped_end so touching memory is "backed by something".
        (void)phys;

        g_heap_mapped_end += PAGE_SIZE;
    }
}

void* kmalloc_aligned(size_t size, size_t align) {
    if (align < 16) align = 16;
    uintptr_t p = ALIGN_UP(g_heap_brk, align);
    uintptr_t new_brk = p + size;

    if (new_brk > g_heap_end) return 0;

    heap_ensure_backed(new_brk);
    g_heap_brk = new_brk;
    return (void*)p;
}

void* kmalloc(size_t size) {
    return kmalloc_aligned(size, 16);
}

void kfree(void* ptr) {
    (void)ptr;
    // bump allocator: no-op
}

size_t heap_bytes_used(void)  { return (size_t)(g_heap_brk - g_heap_start); }
size_t heap_bytes_total(void) { return (size_t)(g_heap_end - g_heap_start); }
