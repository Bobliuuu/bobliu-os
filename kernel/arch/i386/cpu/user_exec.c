// loads an ELF into a brand new user address space
#include <stdint.h>
#include <string.h>
#include <kernel/user_exec.h>
#include <kernel/proc.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/elf.h>
#include <arch/i386/paging.h>
#include <arch/i386/pmm.h>

#define MAX_ELF (512u * 1024u)

static uint32_t align_down(uint32_t x) { return x & 0xFFFFF000u; }
static uint32_t align_up(uint32_t x)   { return (x + 0xFFFu) & 0xFFFFF000u; }

// make sure the page holding va is mapped (zeroed) in dir
static int ensure_page(page_directory_t dir, uint32_t va) {
    if (paging_translate_in(dir, va)) return 0;
    uintptr_t f = pmm_alloc_frame();
    if (!f) return -1;
    memset((void*)f, 0, PAGE_SIZE);
    return paging_map_in(dir, va, f, P_PRESENT | P_RW | P_USER);
}

// copy into a (not currently loaded) address space through the identity map
static void copy_to_dir(page_directory_t dir, uint32_t va, const uint8_t* src, uint32_t n) {
    while (n) {
        uint32_t off = va & 0xFFF;
        uint32_t chunk = PAGE_SIZE - off;
        if (chunk > n) chunk = n;
        uint32_t phys = paging_translate_in(dir, va);
        memcpy((void*)phys, src, chunk);
        va += chunk; src += chunk; n -= chunk;
    }
}

int user_load(const char* path, page_directory_t* out_pd, uint32_t* entry) {
    vfs_stat_t st;
    if (vfs_stat(path, &st) < 0 || st.is_dir || st.size < sizeof(Elf32_Ehdr) || st.size > MAX_ELF)
        return -1;

    int fd = vfs_open(path);
    if (fd < 0) return -1;

    uint8_t* img = (uint8_t*)kmalloc(st.size);
    if (!img) { vfs_close(fd); return -1; }

    uint32_t got = 0;
    while (got < st.size) {
        int r = vfs_read(fd, img + got, st.size - got);
        if (r <= 0) break;
        got += (uint32_t)r;
    }
    vfs_close(fd);
    if (got != st.size) { kfree(img); return -1; }

    Elf32_Ehdr* E = (Elf32_Ehdr*)img;
    if (E->e_ident[0] != 0x7F || E->e_ident[1] != 'E' || E->e_ident[2] != 'L' ||
        E->e_ident[3] != 'F' || E->e_ident[4] != 1 || E->e_ident[5] != 1 ||
        E->e_type != 2 /* ET_EXEC */ || E->e_machine != 3 /* EM_386 */ ||
        E->e_phentsize != sizeof(Elf32_Phdr) ||
        (uint64_t)E->e_phoff + (uint64_t)E->e_phnum * sizeof(Elf32_Phdr) > got) {
        kfree(img);
        return -1;
    }
    Elf32_Phdr* P = (Elf32_Phdr*)(img + E->e_phoff);

    page_directory_t dir = paging_clone_directory(paging_kernel_directory());
    int ok = 1;

    for (uint16_t i = 0; i < E->e_phnum && ok; i++) {
        if (P[i].p_type != PT_LOAD || P[i].p_memsz == 0) continue;

        uint64_t end = (uint64_t)P[i].p_vaddr + P[i].p_memsz;
        if (P[i].p_vaddr < USER_LO || end > USER_HI - USER_STACK_PAGES * PAGE_SIZE ||
            P[i].p_filesz > P[i].p_memsz ||
            (uint64_t)P[i].p_offset + P[i].p_filesz > got) { ok = 0; break; }

        for (uint32_t va = align_down(P[i].p_vaddr); va < align_up((uint32_t)end); va += PAGE_SIZE) {
            if (ensure_page(dir, va) < 0) { ok = 0; break; }
        }
        if (ok) copy_to_dir(dir, P[i].p_vaddr, img + P[i].p_offset, P[i].p_filesz);
    }

    for (uint32_t i = 1; ok && i <= USER_STACK_PAGES; i++) {
        if (ensure_page(dir, USER_STACK_TOP - i * PAGE_SIZE) < 0) ok = 0;
    }

    uint32_t ent = E->e_entry;
    kfree(img);

    if (!ok || ent < USER_LO || ent >= USER_HI) {
        paging_free_user(dir, 1);
        return -1;
    }
    *out_pd = dir;
    *entry = ent;
    return 0;
}
