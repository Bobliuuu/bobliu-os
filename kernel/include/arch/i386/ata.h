#pragma once
#include <stdint.h>

#define ATA_SECTOR 512u

/* probe the primary bus master; returns sector count (LBA28) or 0 if no disk */
uint32_t ata_init(void);
uint32_t ata_sectors(void);
/* polled PIO, one sector at a time. 0 on success, -1 on error/timeout */
int ata_read(uint32_t lba, void* buf);
int ata_write(uint32_t lba, const void* buf);
