// ATA PIO driver for the primary bus, master drive. no irqs, no dma: we
// disable the drive's interrupt line and poll the status register.
#include <arch/i386/ata.h>
#include <arch/i386/portio.h>
#include <stdio.h>

#define IO_BASE  0x1F0
#define CTRL     0x3F6

#define R_DATA   (IO_BASE + 0)
#define R_COUNT  (IO_BASE + 2)
#define R_LBA0   (IO_BASE + 3)
#define R_LBA1   (IO_BASE + 4)
#define R_LBA2   (IO_BASE + 5)
#define R_DRIVE  (IO_BASE + 6)
#define R_CMD    (IO_BASE + 7)   /* write: command, read: status */

#define ST_ERR 0x01
#define ST_DRQ 0x08
#define ST_DF  0x20
#define ST_BSY 0x80

#define CMD_READ     0x20
#define CMD_WRITE    0x30
#define CMD_FLUSH    0xE7
#define CMD_IDENTIFY 0xEC

static uint32_t g_sectors = 0;

// ~400ns delay: four status reads
static void delay400(void) {
    for (int i = 0; i < 4; i++) inb(CTRL);
}

// wait for BSY to clear; returns last status, or -1 on timeout
static int wait_not_busy(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(R_CMD);
        if (!(s & ST_BSY)) return s;
    }
    return -1;
}

// wait until the drive wants / has data (DRQ) or reports an error
static int wait_drq(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(R_CMD);
        if (s & (ST_ERR | ST_DF)) return -1;
        if (!(s & ST_BSY) && (s & ST_DRQ)) return 0;
    }
    return -1;
}

uint32_t ata_init(void) {
    g_sectors = 0;
    outb(CTRL, 0x02);                  // nIEN: no interrupts from the drive
    outb(R_DRIVE, 0xA0);               // master
    delay400();

    if (inb(R_CMD) == 0xFF) return 0;  // floating bus: nothing attached

    outb(R_COUNT, 0);
    outb(R_LBA0, 0);
    outb(R_LBA1, 0);
    outb(R_LBA2, 0);
    outb(R_CMD, CMD_IDENTIFY);
    if (inb(R_CMD) == 0) return 0;     // no drive
    if (wait_not_busy() < 0) return 0;
    if (inb(R_LBA1) || inb(R_LBA2)) return 0;   // ATAPI/SATA signature, not a plain ata disk
    if (wait_drq() < 0) return 0;

    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(R_DATA);
    g_sectors = ((uint32_t)id[61] << 16) | id[60];
    return g_sectors;
}

uint32_t ata_sectors(void) { return g_sectors; }

static int setup(uint32_t lba, uint8_t cmd) {
    if (lba >= g_sectors) return -1;
    if (wait_not_busy() < 0) return -1;
    outb(R_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));   // master, LBA mode
    outb(R_COUNT, 1);
    outb(R_LBA0, (uint8_t)lba);
    outb(R_LBA1, (uint8_t)(lba >> 8));
    outb(R_LBA2, (uint8_t)(lba >> 16));
    outb(R_CMD, cmd);
    return 0;
}

int ata_read(uint32_t lba, void* buf) {
    if (setup(lba, CMD_READ) < 0) return -1;
    delay400();
    if (wait_drq() < 0) return -1;
    uint16_t* w = (uint16_t*)buf;
    for (int i = 0; i < 256; i++) w[i] = inw(R_DATA);
    return 0;
}

int ata_write(uint32_t lba, const void* buf) {
    if (setup(lba, CMD_WRITE) < 0) return -1;
    delay400();
    if (wait_drq() < 0) return -1;
    const uint16_t* w = (const uint16_t*)buf;
    for (int i = 0; i < 256; i++) outw(R_DATA, w[i]);
    outb(R_CMD, CMD_FLUSH);            // make sure it hits the platter
    return wait_not_busy() < 0 ? -1 : 0;
}
