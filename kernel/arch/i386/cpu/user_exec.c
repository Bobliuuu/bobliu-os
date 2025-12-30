// arch/i386/cpu/user_exec.c
#include <stdint.h>
#include <kernel/user_exec.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/panic.h>
#include <arch/i386/paging.h>
#include <arch/i386/usermode.h>
#include <kernel/elf.h>          // Elf32_Ehdr/Elf32_Phdr + PT_LOAD
#include <arch/i386/user_bouncing.h>
#include <stdio.h>

#define PAGE_SIZE 0x1000u

// user stack config
#define USTACK_TOP   0x02000000u   // 32MB
#define PAGE_SIZE    0x1000u
#define USER_STACK_PAGES  4
#define USER_STACK_TOP   0x02000000u
#define USER_STACK_PAGES 4u

// max ELF size we slurp from initrd
#define MAX_ELF (512u * 1024u)

static uint32_t align_down(uint32_t x) { return x & 0xFFFFF000u; }
static uint32_t align_up(uint32_t x)   { return (x + 0xFFFu) & 0xFFFFF000u; }

static inline uint32_t read_cr3(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

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
    uint32_t user_stack_top;
} user_image_t;

static int elf_load_image(const char* path, page_directory_t dir, user_image_t* out) {
    int fd = vfs_open(path);
    if (fd < 0) return -1;

    Elf32_Ehdr eh;
    if (read_all(fd, &eh, sizeof(eh)) < 0) { vfs_close(fd); return -1; }

    if (eh.e_ident[0] != 0x7F || eh.e_ident[1] != 'E' ||
        eh.e_ident[2] != 'L'  || eh.e_ident[3] != 'F') {
        vfs_close(fd);
        return -1;
    }
    if (eh.e_ident[4] != 1 || eh.e_ident[5] != 1) { // ELF32, little-endian
        vfs_close(fd);
        return -1;
    }

    // Slurp file
    uint8_t* img = (uint8_t*)kmalloc(MAX_ELF);
    if (!img) { vfs_close(fd); return -1; }

    // copy header we already read
    for (uint32_t i = 0; i < (uint32_t)sizeof(eh); i++) img[i] = ((uint8_t*)&eh)[i];

    uint32_t file_sz = (uint32_t)sizeof(eh);
    while (file_sz < MAX_ELF) {
        int r = vfs_read(fd, img + file_sz, MAX_ELF - file_sz);
        if (r <= 0) break;
        file_sz += (uint32_t)r;
    }
    vfs_close(fd);

    Elf32_Ehdr* E = (Elf32_Ehdr*)img;

    // Program header bounds check
    if ((uint32_t)E->e_phoff + (uint32_t)E->e_phnum * (uint32_t)sizeof(Elf32_Phdr) > file_sz) {
        kfree(img);
        return -1;
    }

    Elf32_Phdr* P = (Elf32_Phdr*)(img + (uint32_t)E->e_phoff);

    // Save kernel dir so we can restore after copying
    page_directory_t kdir = paging_kernel_directory();

    // --- Map all PT_LOAD segments into 'dir' first ---
    for (uint16_t i = 0; i < E->e_phnum; i++) {
        if (P[i].p_type != PT_LOAD) continue;
        if (P[i].p_memsz == 0) continue;

        // file bounds for this segment
        if ((uint32_t)P[i].p_offset + (uint32_t)P[i].p_filesz > file_sz) {
            kfree(img);
            return -1;
        }

        uint32_t seg_start = align_down((uint32_t)P[i].p_vaddr);
        uint32_t seg_end   = align_up((uint32_t)P[i].p_vaddr + (uint32_t)P[i].p_memsz);

        // permissions (keep it simple: RW for now; later respect PF_W)
        uint32_t map_flags = P_PRESENT | P_USER | P_RW;

        for (uint32_t va = seg_start; va < seg_end; va += PAGE_SIZE) {
            if (paging_alloc_map_in(dir, va, map_flags) < 0) {
                kfree(img);
                return -1;
            }
        }
    }

    for (uint32_t i = 1; i <= USER_STACK_PAGES; i++) {
        uint32_t va = USER_STACK_TOP - i * PAGE_SIZE;
        if (paging_alloc_map_in(dir, va, P_PRESENT | P_RW | P_USER) < 0) {
            kfree(img);
            return -1;
        }
    }

    // --- Now switch to 'dir' ONCE and actually copy/zero segments ---
    paging_switch_directory(dir);

    for (uint16_t i = 0; i < E->e_phnum; i++) {
        if (P[i].p_type != PT_LOAD) continue;
        if (P[i].p_memsz == 0) continue;

        uint8_t* dst = (uint8_t*)(uintptr_t)P[i].p_vaddr;
        uint8_t* src = img + (uint32_t)P[i].p_offset;

        for (uint32_t b = 0; b < (uint32_t)P[i].p_filesz; b++) dst[b] = src[b];
        for (uint32_t b = (uint32_t)P[i].p_filesz; b < (uint32_t)P[i].p_memsz; b++) dst[b] = 0;
    }

    // Restore kernel directory
    paging_switch_directory(kdir);

    out->entry = (uint32_t)E->e_entry;
    out->user_stack_top = USER_STACK_TOP;

    kfree(img);

    printf("[elf] entry=%x user_stack_top=%x pages=%u\n",
       out->entry, out->user_stack_top, (unsigned)USER_STACK_PAGES);
    return 0;
}

int user_exec(const char* path) {
    printf("user_exec: path='%s'\n", path);

    page_directory_t kdir = paging_kernel_directory();
    page_directory_t pdir = paging_clone_directory(kdir);

    user_image_t img;
    if (elf_load_image(path, pdir, &img) < 0) return -1;

    paging_switch_directory(pdir);
    g_exec_kcr3 = read_cr3();
    int rc = exec_enter_usermode(img.entry, img.user_stack_top);

    // Optional but fine now:
    paging_switch_directory(kdir);

    return rc;
}
