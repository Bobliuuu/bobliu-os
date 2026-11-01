// bochs dispi framebuffer (qemu -vga std). found through pci, switched on demand
#include <stdint.h>
#include <arch/i386/portio.h>
#include <arch/i386/paging.h>
#include <arch/i386/fb.h>
#include <arch/i386/irq.h>

#define VBE_INDEX 0x01CE
#define VBE_DATA  0x01CF
#define VBE_ID 0
#define VBE_XRES 1
#define VBE_YRES 2
#define VBE_BPP 3
#define VBE_ENABLE 4
#define VBE_ENABLED 0x01
#define VBE_LFB 0x40
#define VBE_NOCLEAR 0x80

/* legacy vga text/font memory shares the first bytes of vram with the linear fb, so keep a copy
   (seen through a kernel window in the shared kstack pde, visible from every address space) */
#define SAVE_BYTES 0x20000u
#define KWIN 0xE0300000u
static uint8_t g_save[SAVE_BYTES];

#define FB_W 640
#define FB_H 480
#define FB_BPP 32

static void vbe_w(uint16_t i, uint16_t v) { outw(VBE_INDEX, i); outw(VBE_DATA, v); }
static uint16_t vbe_r(uint16_t i) { outw(VBE_INDEX, i); return inw(VBE_DATA); }

static uint32_t pci_r32(int bus, int dev, int fn, int off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}

// legacy vga register file, so text mode comes back exactly as grub left it
static uint8_t r_misc, r_seq[5], r_crtc[25], r_gc[9], r_ac[21];

static void vga_save(void) {
    r_misc = inb(0x3CC);
    for (int i = 0; i < 5; i++)  { outb(0x3C4, i); r_seq[i] = inb(0x3C5); }
    for (int i = 0; i < 25; i++) { outb(0x3D4, i); r_crtc[i] = inb(0x3D5); }
    for (int i = 0; i < 9; i++)  { outb(0x3CE, i); r_gc[i] = inb(0x3CF); }
    for (int i = 0; i < 21; i++) { (void)inb(0x3DA); outb(0x3C0, i); r_ac[i] = inb(0x3C1); }
    (void)inb(0x3DA); outb(0x3C0, 0x20);
}

static void vga_restore(void) {
    outb(0x3C2, r_misc);
    outb(0x3C4, 0); outb(0x3C5, 1);                       // sync reset
    for (int i = 1; i < 5; i++) { outb(0x3C4, i); outb(0x3C5, r_seq[i]); }
    outb(0x3C4, 0); outb(0x3C5, 3);
    outb(0x3D4, 0x11); outb(0x3D5, r_crtc[0x11] & 0x7F);  // unlock crtc 0-7
    for (int i = 0; i < 25; i++) { outb(0x3D4, i); outb(0x3D5, i == 0x11 ? (r_crtc[i] & 0x7F) : r_crtc[i]); }
    outb(0x3D4, 0x11); outb(0x3D5, r_crtc[0x11]);
    for (int i = 0; i < 9; i++)  { outb(0x3CE, i); outb(0x3CF, r_gc[i]); }
    for (int i = 0; i < 21; i++) { (void)inb(0x3DA); outb(0x3C0, i); outb(0x3C0, r_ac[i]); }
    (void)inb(0x3DA); outb(0x3C0, 0x20);                  // video on
}

static uint32_t g_phys;      // framebuffer physical address (0 = not looked up / none)
static int g_probed;
static void* g_owner;
static uint32_t g_pages;

static void probe(void) {
    g_probed = 1;
    for (int dev = 0; dev < 32; dev++) {
        uint32_t id = pci_r32(0, dev, 0, 0);
        if ((id & 0xFFFF) == 0x1234 && (id >> 16) == 0x1111) {
            uint32_t bar = pci_r32(0, dev, 0, 0x10);
            if (!(bar & 1)) g_phys = bar & 0xFFFFFFF0u;
            if (g_phys)
                for (uint32_t o = 0; o < SAVE_BYTES; o += PAGE_SIZE)
                    if (paging_map(KWIN + o, g_phys + o, P_RW | P_DEV) < 0) g_phys = 0;
            return;
        }
    }
}

int fb_acquire(void* owner, uint32_t* info) {
    if (!g_probed) probe();
    if (!g_phys || vbe_r(VBE_ID) < 0xB0C0) return -1;
    if (g_owner && g_owner != owner) return -1;

    uint32_t fl = irq_save();
    for (uint32_t i = 0; i < SAVE_BYTES; i++) g_save[i] = ((volatile uint8_t*)KWIN)[i];
    vga_save();
    vbe_w(VBE_ENABLE, 0);
    vbe_w(VBE_XRES, FB_W);
    vbe_w(VBE_YRES, FB_H);
    vbe_w(VBE_BPP, FB_BPP);
    vbe_w(VBE_ENABLE, VBE_ENABLED | VBE_LFB | VBE_NOCLEAR);
    g_owner = owner;
    irq_restore(fl);

    g_pages = (FB_W * FB_H * (FB_BPP / 8) + PAGE_SIZE - 1) / PAGE_SIZE;
    page_directory_t pd = { (uint32_t*)paging_current_pd_virt(), (uint32_t*)paging_current_pd_virt() };
    for (uint32_t i = 0; i < g_pages; i++)
        if (paging_map_in(pd, USER_FB_BASE + i * PAGE_SIZE, g_phys + i * PAGE_SIZE, P_USER | P_RW | P_DEV) < 0) {
            fb_release(owner);
            return -1;
        }
    info[0] = FB_W; info[1] = FB_H; info[2] = FB_W * (FB_BPP / 8); info[3] = FB_BPP; info[4] = USER_FB_BASE;
    return 0;
}

void fb_release(void* owner) {
    if (!g_owner || g_owner != owner) return;
    uint32_t fl = irq_save();
    vbe_w(VBE_ENABLE, 0);           // back to vga text mode
    vga_restore();
    for (uint32_t i = 0; i < SAVE_BYTES; i++) ((volatile uint8_t*)KWIN)[i] = g_save[i];
    g_owner = 0;
    irq_restore(fl);
}
