// first-fit malloc on top of sbrk. every block (used or free) sits in one
// address-ordered list, so a free can merge with both neighbours.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

typedef struct blk {
    size_t size;        // payload bytes, multiple of 8
    struct blk* next;   // next block in address order
    struct blk* prev;
    int free;
} blk_t;

#define HDR   sizeof(blk_t)
#define ALIGN 8u
#define GROW  16384u

static volatile int g_lock;      // threads share the heap
static void lock(void)   { while (__sync_lock_test_and_set(&g_lock, 1)) sched_yield(); }
static void unlock(void) { __sync_lock_release(&g_lock); }

static blk_t* g_head = 0;
static blk_t* g_tail = 0;

static blk_t* grow(size_t need) {
    size_t total = need + HDR;
    if (total < GROW) total = GROW;
    total = (total + 4095u) & ~4095u;

    void* p = sbrk((int)total);
    if (p == (void*)-1) return 0;

    blk_t* b = (blk_t*)p;
    b->size = total - HDR;
    b->free = 1;
    b->next = 0;
    b->prev = g_tail;
    if (g_tail) g_tail->next = b; else g_head = b;
    g_tail = b;

    // merge with a free tail
    if (b->prev && b->prev->free) {
        blk_t* a = b->prev;
        a->size += HDR + b->size;
        a->next = 0;
        g_tail = a;
        b = a;
    }
    return b;
}

static void* malloc_locked(size_t size) {
    if (size == 0) size = 1;
    size = (size + ALIGN - 1) & ~(ALIGN - 1);

    blk_t* b;
    for (b = g_head; b; b = b->next)
        if (b->free && b->size >= size) break;
    if (!b) {
        b = grow(size);
        if (!b || b->size < size) return 0;
    }

    // split off the tail if it can hold a header plus something useful
    if (b->size >= size + HDR + ALIGN) {
        blk_t* n = (blk_t*)((uint8_t*)b + HDR + size);
        n->size = b->size - size - HDR;
        n->free = 1;
        n->next = b->next;
        n->prev = b;
        if (b->next) b->next->prev = n; else g_tail = n;
        b->next = n;
        b->size = size;
    }
    b->free = 0;
    return (uint8_t*)b + HDR;
}

static void free_locked(void* p) {
    if (!p) return;
    blk_t* b = (blk_t*)((uint8_t*)p - HDR);
    b->free = 1;

    if (b->next && b->next->free) {         // absorb the next block
        blk_t* n = b->next;
        b->size += HDR + n->size;
        b->next = n->next;
        if (n->next) n->next->prev = b; else g_tail = b;
    }
    if (b->prev && b->prev->free) {         // be absorbed by the previous one
        blk_t* a = b->prev;
        a->size += HDR + b->size;
        a->next = b->next;
        if (b->next) b->next->prev = a; else g_tail = a;
    }
}

void* malloc(size_t size) {
    lock();
    void* p = malloc_locked(size);
    unlock();
    return p;
}

void free(void* p) {
    lock();
    free_locked(p);
    unlock();
}

void* calloc(size_t n, size_t size) {
    if (size && n > (size_t)-1 / size) return 0;
    void* p = malloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}

void* realloc(void* p, size_t size) {
    if (!p) return malloc(size);
    if (size == 0) { free(p); return 0; }
    blk_t* b = (blk_t*)((uint8_t*)p - HDR);
    if (b->size >= size) return p;
    void* q = malloc(size);
    if (!q) return 0;
    memcpy(q, p, b->size);
    free(p);
    return q;
}
