#pragma once
#include <stdint.h>

#define MULTIBOOT1_MAGIC 0x2BADB002u

/* flags bits */
#define MULTIBOOT1_INFO_MEM     (1u << 0)
#define MULTIBOOT1_INFO_CMDLINE (1u << 2)
#define MULTIBOOT1_INFO_MODS    (1u << 3)
#define MULTIBOOT1_INFO_MMAP    (1u << 6)

/* mmap entry (MUST be packed) */
typedef struct multiboot_mmap_entry {
    uint32_t size;     /* size of entry excluding this field */
    uint64_t addr;
    uint64_t len;
    uint32_t type;     /* 1 = available RAM */
} __attribute__((packed)) multiboot_mmap_entry_t;

/* module entry */
typedef struct multiboot_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;

/* multiboot info (layout must match spec up to mmap fields) */
typedef struct multiboot_info {
    uint32_t flags;

    uint32_t mem_lower;
    uint32_t mem_upper;

    uint32_t boot_device;
    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;

    union {
        struct {
            uint32_t tabsize;
            uint32_t strsize;
            uint32_t addr;
            uint32_t reserved;
        } aout_sym;

        struct {
            uint32_t num;
            uint32_t size;
            uint32_t addr;
            uint32_t shndx;
        } elf_sec;
    } u;

    uint32_t mmap_length;
    uint32_t mmap_addr;
} multiboot_info_t;

/* helpers */
void multiboot1_init(uint32_t magic, void* mbi_ptr);
const char* multiboot1_cmdline(void);
multiboot_info_t* multiboot1_info(void);
uint32_t multiboot1_magic(void);
