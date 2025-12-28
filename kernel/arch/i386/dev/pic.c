// pic.c
#include <stdint.h>
#include <arch/i386/portio.h>

#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1

void pic_set_mask(uint8_t irq_line) {
    uint16_t port = (irq_line < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq_line >= 8) irq_line -= 8;
    uint8_t value = inb(port) | (1 << irq_line);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq_line) {
    uint16_t port = (irq_line < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq_line >= 8) irq_line -= 8;
    uint8_t value = inb(port) & ~(1 << irq_line);
    outb(port, value);
}

void pic_remap(int offset1, int offset2) {
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);

    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();

    outb(0x21, offset1); io_wait();
    outb(0xA1, offset2); io_wait();

    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();

    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    outb(0x21, a1);
    outb(0xA1, a2);
}
