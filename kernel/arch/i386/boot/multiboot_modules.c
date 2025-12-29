#include <arch/i386/multiboot_modules.h>
#include <arch/i386/multiboot_1.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

multiboot_info_t* multiboot1_info(void);   // you already have this

int mb1_get_module(int index, mb1_module_view_t* out) {
    multiboot_info_t* mbi = multiboot1_info();
    if (!mbi) return -1;
    if (!(mbi->flags & MULTIBOOT1_INFO_MODS)) return -1;
    if ((uint32_t)index >= mbi->mods_count) return -1;

    multiboot_module_t* mods = (multiboot_module_t*)(uintptr_t)mbi->mods_addr;
    out->start = (uintptr_t)mods[index].mod_start;
    out->end   = (uintptr_t)mods[index].mod_end;
    out->string = (const char*)(uintptr_t)mods[index].string; // GRUB sets this to the module string
    return 0;
}

int mb1_find_module_by_string(const char* needle, mb1_module_view_t* out) {
    multiboot_info_t* mbi = multiboot1_info();
    if (!mbi) return -1;
    if (!(mbi->flags & MULTIBOOT1_INFO_MODS)) return -1;

    multiboot_module_t* mods = (multiboot_module_t*)(uintptr_t)mbi->mods_addr;
    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        const char* s = (const char*)(uintptr_t)mods[i].string;
        if (s && strstr(s, needle)) {
            out->start = (uintptr_t)mods[i].mod_start;
            out->end   = (uintptr_t)mods[i].mod_end;
            out->string = s;
            return 0;
        }
    }
    return -1;
}

void multiboot1_dump_modules(multiboot_info_t* mbi) {
    if (!mbi) { printf("mb: mbi=null\n"); return; }

    printf("mb: flags=%x\n", mbi->flags);

    if (!(mbi->flags & MULTIBOOT1_INFO_MODS)) {
        printf("mb: no modules\n");
        return;
    }

    printf("mb: mods_count=%u mods_addr=%x\n",
           (uint32_t)mbi->mods_count,
           (uint32_t)mbi->mods_addr);

    multiboot_module_t* mods =
        (multiboot_module_t*)(uintptr_t)mbi->mods_addr;

    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        const char* s = (const char*)(uintptr_t)mods[i].string;
        printf("mb: mod[%u] %x..%x (%u bytes) str='%s'\n",
               i,
               (uint32_t)mods[i].mod_start,
               (uint32_t)mods[i].mod_end,
               (uint32_t)(mods[i].mod_end - mods[i].mod_start),
               s ? s : "(null)");
    }
}

int multiboot1_find_module(multiboot_info_t* mbi,
                           const char* want_substr,
                           uintptr_t* out_start,
                           uintptr_t* out_end) {
    if (!mbi || !(mbi->flags & MULTIBOOT1_INFO_MODS)) return 0;

    multiboot_module_t* mods = (multiboot_module_t*)(uintptr_t)mbi->mods_addr;

    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        const char* s = (const char*)(uintptr_t)mods[i].string;
        if (!want_substr || !want_substr[0]) {
            *out_start = (uintptr_t)mods[i].mod_start;
            *out_end   = (uintptr_t)mods[i].mod_end;
            return 1;
        }
        if (s && strstr(s, want_substr)) {
            *out_start = (uintptr_t)mods[i].mod_start;
            *out_end   = (uintptr_t)mods[i].mod_end;
            return 1;
        }
    }
    return 0;
}