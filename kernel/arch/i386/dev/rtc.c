// cmos real time clock: read once at boot, then wall time = boot epoch + uptime
#include <stdint.h>
#include <arch/i386/portio.h>
#include <arch/i386/timer.h>

static uint32_t g_boot_epoch = 0;

static uint8_t cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int updating(void) { return cmos(0x0A) & 0x80; }

static uint8_t bcd(uint8_t v) { return (uint8_t)((v & 0x0F) + (v >> 4) * 10); }

// days from 1970-01-01 to y-m-d (proleptic gregorian)
static uint32_t days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (uint32_t)(era * 146097 + (int)doe - 719468);
}

void rtc_init(void) {
    uint8_t s, mi, h, d, mo, y, s2, mi2, h2, d2, mo2, y2;
    // re-read until two reads agree, so we never catch it mid-update
    do {
        while (updating());
        s = cmos(0x00); mi = cmos(0x02); h = cmos(0x04); d = cmos(0x07); mo = cmos(0x08); y = cmos(0x09);
        while (updating());
        s2 = cmos(0x00); mi2 = cmos(0x02); h2 = cmos(0x04); d2 = cmos(0x07); mo2 = cmos(0x08); y2 = cmos(0x09);
    } while (s != s2 || mi != mi2 || h != h2 || d != d2 || mo != mo2 || y != y2);

    uint8_t regb = cmos(0x0B);
    int pm = h & 0x80;
    h &= 0x7F;
    if (!(regb & 4)) { s = bcd(s); mi = bcd(mi); h = bcd(h); d = bcd(d); mo = bcd(mo); y = bcd(y); }
    if (!(regb & 2)) {                      // 12 hour clock
        h %= 12;
        if (pm) h += 12;
    }

    uint32_t days = days_from_civil(2000 + y, mo, d);
    g_boot_epoch = days * 86400u + h * 3600u + mi * 60u + s - timer_ticks() / TIMER_HZ;
}

uint32_t rtc_boot_epoch(void) { return g_boot_epoch; }

uint32_t wall_time(void) { return g_boot_epoch + timer_ticks() / TIMER_HZ; }
