#include <kernel/heap.h>
#include <arch/i386/pmm.h>

extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);

/*
 * first-fit free-list heap with split + coalesce.
 * the region is identity mapped and its frames are reserved in the pmm so
 * nobody else hands them out.
 */

typedef struct blk {
    size_t size;        // payload bytes
    struct blk* next;   // next block in address order
    uint32_t free;
    uint32_t magic;
} blk_t;                // 16 bytes on i386

#define HDR       ((size_t)sizeof(blk_t))
#define MAGIC     0xB0B11AAAu
#define ALIGN_UP(x,a) (((x) + ((a)-1)) & ~((a)-1))

static blk_t* g_head = 0;
static uintptr_t g_heap_start = 0, g_heap_end = 0;
static size_t g_used = 0;

void heap_init(uintptr_t heap_start, size_t heap_size) {
    g_heap_start = ALIGN_UP(heap_start, 4096);
    g_heap_end   = g_heap_start + (heap_size & ~(size_t)4095);

    pmm_reserve_range(g_heap_start, g_heap_end - g_heap_start);

    g_head = (blk_t*)g_heap_start;
    g_head->size  = (g_heap_end - g_heap_start) - HDR;
    g_head->next  = 0;
    g_head->free  = 1;
    g_head->magic = MAGIC;

    vga_print("heap: start=");
    vga_print_hex((uint32_t)g_heap_start);
    vga_print(" end=");
    vga_print_hex((uint32_t)g_heap_end);
    vga_print("\n");
}

void* kmalloc(size_t size) {
    if (!g_head) return 0;
    size = ALIGN_UP(size ? size : 1, 16);

    for (blk_t* b = g_head; b; b = b->next) {
        if (!b->free || b->size < size) continue;

        // split if the leftover can hold a header + 16 bytes
        if (b->size >= size + HDR + 16) {
            blk_t* n = (blk_t*)((uint8_t*)b + HDR + size);
            n->size  = b->size - size - HDR;
            n->next  = b->next;
            n->free  = 1;
            n->magic = MAGIC;
            b->size  = size;
            b->next  = n;
        }
        b->free = 0;
        g_used += b->size;
        return (uint8_t*)b + HDR;
    }
    return 0;
}

// only used for align <= 16 callers today; bigger alignments are not supported
void* kmalloc_aligned(size_t size, size_t align) {
    (void)align;
    return kmalloc(size);
}

void kfree(void* ptr) {
    if (!ptr) return;
    blk_t* b = (blk_t*)((uint8_t*)ptr - HDR);
    if (b->magic != MAGIC || b->free) {
        vga_print("kfree: bad/double free\n");
        return;
    }
    b->free = 1;
    g_used -= b->size;

    // coalesce forward
    while (b->next && b->next->free) {
        b->size += HDR + b->next->size;
        b->next = b->next->next;
    }
    // coalesce backward
    for (blk_t* p = g_head; p && p != b; p = p->next) {
        if (p->next == b && p->free) {
            p->size += HDR + b->size;
            p->next = b->next;
            break;
        }
    }
}

size_t heap_bytes_used(void)  { return g_used; }
size_t heap_bytes_total(void) { return g_heap_end - g_heap_start; }
