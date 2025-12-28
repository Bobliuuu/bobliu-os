#include <stdint.h>
#include <stddef.h>
#include <arch/i386/multiboot_1.h>
#include <stdio.h>

void pmm_init(uint32_t multiboot_magic, uintptr_t multiboot_info_phys);

uintptr_t pmm_alloc_frame(void);          /* returns physical address, 0 on OOM */
void      pmm_free_frame(uintptr_t paddr);

/* stats */
uint32_t  pmm_total_frames(void);
uint32_t  pmm_free_frames(void);
uint32_t  pmm_used_frames(void);
