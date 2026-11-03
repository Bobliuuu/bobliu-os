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

// read exactly n bytes at the current file offset
static int read_full(int fd, void* dst, uint32_t n) {
    uint8_t* d = (uint8_t*)dst;
    uint32_t got = 0;
    while (got < n) {
        int r = vfs_read(fd, d + got, n - got);
        if (r <= 0) return -1;
        got += (uint32_t)r;
    }
    return 0;
}

// the file is streamed straight into the new address space: no size limit besides memory
int user_load(const char* path, char** argv, int argc, char** envp, int envc,
              page_directory_t* out_pd, uint32_t* entry, uint32_t* out_sp) {
    vfs_stat_t st;
    if (vfs_stat(path, &st) < 0 || st.is_dir || st.size < sizeof(Elf32_Ehdr)) return -1;

    int fd = vfs_open(path);
    if (fd < 0) return -1;

    Elf32_Ehdr eh;
    if (read_full(fd, &eh, sizeof(eh)) < 0) { vfs_close(fd); return -1; }
    Elf32_Ehdr* E = &eh;
    if (E->e_ident[0] != 0x7F || E->e_ident[1] != 'E' || E->e_ident[2] != 'L' ||
        E->e_ident[3] != 'F' || E->e_ident[4] != 1 || E->e_ident[5] != 1 ||
        E->e_type != 2 /* ET_EXEC */ || E->e_machine != 3 /* EM_386 */ ||
        E->e_phentsize != sizeof(Elf32_Phdr) || E->e_phnum == 0 || E->e_phnum > 16 ||
        (uint64_t)E->e_phoff + (uint64_t)E->e_phnum * sizeof(Elf32_Phdr) > st.size) {
        vfs_close(fd);
        return -1;
    }
    Elf32_Phdr P[16];
    if (vfs_seek(fd, (int32_t)E->e_phoff, 0) < 0 || read_full(fd, P, E->e_phnum * sizeof(Elf32_Phdr)) < 0) {
        vfs_close(fd);
        return -1;
    }

    page_directory_t dir = paging_clone_directory(paging_kernel_directory());
    int ok = 1;

    for (uint16_t i = 0; i < E->e_phnum && ok; i++) {
        if (P[i].p_type != PT_LOAD || P[i].p_memsz == 0) continue;

        uint64_t end = (uint64_t)P[i].p_vaddr + P[i].p_memsz;
        if (P[i].p_vaddr < USER_LO || end > USER_HI - USER_STACK_PAGES * PAGE_SIZE ||
            P[i].p_filesz > P[i].p_memsz ||
            (uint64_t)P[i].p_offset + P[i].p_filesz > st.size) { ok = 0; break; }

        for (uint32_t va = align_down(P[i].p_vaddr); va < align_up((uint32_t)end); va += PAGE_SIZE) {
            if (ensure_page(dir, va) < 0) { ok = 0; break; }
        }
        if (!ok) break;

        // file bytes land page by page in the frames (the kernel reaches them through the identity map)
        if (vfs_seek(fd, (int32_t)P[i].p_offset, 0) < 0) { ok = 0; break; }
        uint32_t va = P[i].p_vaddr, left = P[i].p_filesz;
        while (left) {
            uint32_t off = va & 0xFFF, chunk = PAGE_SIZE - off;
            if (chunk > left) chunk = left;
            uint32_t phys = paging_translate_in(dir, va);
            if (read_full(fd, (void*)phys, chunk) < 0) { ok = 0; break; }
            va += chunk; left -= chunk;
        }
    }
    vfs_close(fd);

    for (uint32_t i = 1; ok && i <= USER_STACK_PAGES; i++) {
        if (ensure_page(dir, USER_STACK_TOP - i * PAGE_SIZE) < 0) ok = 0;
    }

    uint32_t ent = E->e_entry;

    // initial stack: [argc][argv0..argvN][NULL][env0..envM][NULL] ... strings (crt0 hands these to main)
    uint32_t sp = USER_STACK_TOP;
    if (ok) {
        uint32_t aptr[PROC_MAX_ARGS], eptr[PROC_MAX_ENV];
        if (argc > PROC_MAX_ARGS) argc = PROC_MAX_ARGS;
        if (envc > PROC_MAX_ENV) envc = PROC_MAX_ENV;
        if (!envp) envc = 0;
        for (int i = envc - 1; i >= 0; i--) {
            uint32_t len = (uint32_t)strlen(envp[i]) + 1;
            sp -= len;
            copy_to_dir(dir, sp, (const uint8_t*)envp[i], len);
            eptr[i] = sp;
        }
        for (int i = argc - 1; i >= 0; i--) {
            uint32_t len = (uint32_t)strlen(argv[i]) + 1;
            sp -= len;
            copy_to_dir(dir, sp, (const uint8_t*)argv[i], len);
            aptr[i] = sp;
        }
        sp &= ~15u;
        sp -= 4 * (uint32_t)(argc + 1 + envc + 1);
        uint32_t zero = 0, at = sp;
        for (int i = 0; i < argc; i++, at += 4) copy_to_dir(dir, at, (const uint8_t*)&aptr[i], 4);
        copy_to_dir(dir, at, (const uint8_t*)&zero, 4); at += 4;
        for (int i = 0; i < envc; i++, at += 4) copy_to_dir(dir, at, (const uint8_t*)&eptr[i], 4);
        copy_to_dir(dir, at, (const uint8_t*)&zero, 4);
        sp -= 4;
        uint32_t n = (uint32_t)argc;
        copy_to_dir(dir, sp, (const uint8_t*)&n, 4);
    }

    if (!ok || ent < USER_LO || ent >= USER_HI) {
        paging_free_user(dir, 1);
        return -1;
    }
    *out_pd = dir;
    *entry = ent;
    *out_sp = sp;
    return 0;
}
