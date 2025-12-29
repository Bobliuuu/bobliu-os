#include <stdint.h>
#include <kernel/vfs.h>
#include <arch/i386/paging.h>
#include <kernel/elf.h>

static uint32_t align_down(uint32_t x) { return x & 0xFFFFF000u; }
static uint32_t align_up(uint32_t x)   { return (x + 0xFFFu) & 0xFFFFF000u; }

static int read_all(int fd, void* buf, uint32_t n) {
    uint8_t* p = (uint8_t*)buf;
    uint32_t got = 0;
    while (got < n) {
        int r = vfs_read(fd, p + got, n - got);
        if (r <= 0) return -1;
        got += (uint32_t)r;
    }
    return 0;
}

// minimal: no seek yet -> we read whole file into heap
// (good enough for initrd tiny binaries)
extern void* kmalloc(uint32_t);
extern void  kfree(void*);

typedef struct {
    uint32_t entry;
    uint32_t user_stack_top;
} user_image_t;

int elf_load_from_vfs(const char* path, page_directory_t dir, user_image_t* out) {
    int fd = vfs_open(path);
    if (fd < 0) return -1;

    // Read first page (enough for headers)
    Elf32_Ehdr eh;
    if (read_all(fd, &eh, sizeof(eh)) < 0) { vfs_close(fd); return -1; }

    // validate ELF magic
    if (eh.e_ident[0] != 0x7F || eh.e_ident[1] != 'E' || eh.e_ident[2] != 'L' || eh.e_ident[3] != 'F') {
        vfs_close(fd);
        return -1;
    }
    // 32-bit little endian only
    if (eh.e_ident[4] != 1 || eh.e_ident[5] != 1) {
        vfs_close(fd);
        return -1;
    }

    // You likely don't have vfs_seek -> easiest: read whole file into memory first.
    // For now, implement a simple "slurp file" by reading in chunks until EOF.
    // We'll do a fixed max to keep it minimal.
    const uint32_t MAX_ELF = 512 * 1024;
    uint8_t* img = (uint8_t*)kmalloc(MAX_ELF);
    if (!img) { vfs_close(fd); return -1; }

    // Copy header already read
    for (uint32_t i = 0; i < sizeof(eh); i++) img[i] = ((uint8_t*)&eh)[i];

    uint32_t off = sizeof(eh);
    while (off < MAX_ELF) {
        int r = vfs_read(fd, img + off, MAX_ELF - off);
        if (r <= 0) break;
        off += (uint32_t)r;
    }
    vfs_close(fd);

    Elf32_Ehdr* E = (Elf32_Ehdr*)img;
    Elf32_Phdr* P = (Elf32_Phdr*)(img + E->e_phoff);

    // Map & load PT_LOAD segments
    for (uint16_t i = 0; i < E->e_phnum; i++) {
        if (P[i].p_type != PT_LOAD) continue;

        uint32_t seg_start = align_down(P[i].p_vaddr);
        uint32_t seg_end   = align_up(P[i].p_vaddr + P[i].p_memsz);

        // map pages
        for (uint32_t va = seg_start; va < seg_end; va += 0x1000) {
            if (paging_alloc_map_in(dir, va, P_PRESENT | P_RW | P_USER) < 0) {
                kfree(img);
                return -1;
            }
        }

        // copy file bytes
        uint8_t* dst = (uint8_t*)(uintptr_t)P[i].p_vaddr;
        uint8_t* src = img + P[i].p_offset;

        for (uint32_t b = 0; b < P[i].p_filesz; b++) dst[b] = src[b];

        // zero bss
        for (uint32_t b = P[i].p_filesz; b < P[i].p_memsz; b++) dst[b] = 0;
    }

    // Map user stack (1 page for now)
    uint32_t USTACK_TOP  = 0x00800000u;
    uint32_t USTACK_PAGE = USTACK_TOP - 0x1000u;
    if (paging_alloc_map_in(dir, USTACK_PAGE, P_PRESENT | P_RW | P_USER) < 0) {
        kfree(img);
        return -1;
    }

    out->entry = E->e_entry;
    out->user_stack_top = USTACK_TOP;

    kfree(img);
    return 0;
}
