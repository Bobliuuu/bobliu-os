#include <stdint.h>
#include <kernel/user_exec.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/panic.h>
#include <arch/i386/paging.h>
#include <arch/i386/usermode.h>
#include <stdio.h>
#include <kernel/elf.h>   // the Elf32_Ehdr/Elf32_Phdr structs + PT_LOAD
#include <arch/i386/user_bounce.h>

// pick a simple fixed stack
#define USTACK_TOP    0x00800000u
#define USTACK_PAGES  4
#define PAGE_SIZE     0x1000u

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

typedef struct {
    uint32_t entry;
} user_image_t;

static int elf_load_image(const char* path, page_directory_t dir, user_image_t* out) {
    int fd = vfs_open(path);
    if (fd < 0) return -1;

    // read ELF header first
    Elf32_Ehdr eh;
    if (read_all(fd, &eh, sizeof(eh)) < 0) { vfs_close(fd); return -1; }

    if (eh.e_ident[0] != 0x7F || eh.e_ident[1] != 'E' ||
        eh.e_ident[2] != 'L'  || eh.e_ident[3] != 'F') {
        vfs_close(fd);
        return -1;
    }
    if (eh.e_ident[4] != 1 || eh.e_ident[5] != 1) { // 32-bit, little-endian
        vfs_close(fd);
        return -1;
    }

    // slurp whole file into heap (no seek needed)
    uint8_t* img = (uint8_t*)kmalloc(MAX_ELF);
    if (!img) { vfs_close(fd); return -1; }

    // copy header already read
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

    for (uint16_t i = 0; i < E->e_phnum; i++) {
        if (P[i].p_type != PT_LOAD) continue;

        uint32_t seg_start = align_down(P[i].p_vaddr);
        uint32_t seg_end   = align_up(P[i].p_vaddr + P[i].p_memsz);

        // map segment pages as user
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
        for (uint32_t b = P[i].p_filesz; b < P[i].p_memsz; b++) dst[b] = 0;
    }

    // map 1-page user stack (WRONG)
    // Fix: map user stack (multiple pages)
    for (int i = 1; i <= USTACK_PAGES; i++) {
        uint32_t va = USTACK_TOP - i * PAGE_SIZE;
        if (paging_alloc_map_in(dir, va, P_PRESENT | P_RW | P_USER) < 0) {
            kfree(img);
            return -1;
        }
    }
    
    out->entry = E->e_entry;
    kfree(img);
    return 0;
}

int user_exec(const char* path) {
    // Create a fresh address space: clone kernel mappings + empty user
    page_directory_t kdir = paging_kernel_directory();
    page_directory_t pdir = paging_clone_directory(kdir);

    user_image_t img;
    if (elf_load_image(path, pdir, &img) < 0) {
        return -1;
    }

    // switch to program's page directory
    paging_switch_directory(pdir);

    // panic("enter_user returned");

    printf("entered\n");

    // enter ring3
    return exec_enter_usermode(img.entry, USTACK_TOP);
}
