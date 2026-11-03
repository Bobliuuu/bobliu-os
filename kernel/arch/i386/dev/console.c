// stdin for user programs: the keyboard irq pushes finished lines here
#include <kernel/signal.h>
#include <stdint.h>
#include <string.h>
#include <kernel/proc.h>
#include <arch/i386/irq.h>

#define CON_BUF 1024

static char g_buf[CON_BUF];
static volatile uint32_t g_head = 0, g_tail = 0;   // read at tail, write at head

static uint32_t count(void) { return (g_head - g_tail) % CON_BUF; }

static volatile int g_raw = 0;
// termios-ish flags (Linux values)
#define T_ISIG   0x01
#define T_ICANON 0x02
#define T_ECHO   0x08
#define T_ICRNL  0x100
#define T_VMIN   6
#define T_VTIME  5
static volatile int g_echo = 1, g_isig = 1, g_icrnl = 1, g_vmin = 1;

int console_is_raw(void) { return g_raw; }
int console_echo(void) { return g_echo; }
int console_isig(void) { return g_isig; }
int console_icrnl(void) { return g_icrnl; }

// raw mode: keys arrive as bytes with no echo / line editing; flushes pending input on every switch
void console_set_raw(int on) {
    uint32_t fl = irq_save();
    g_raw = on;   // 1 = bytes, 2 = raw set-1 scancodes (bit 7 = release)
    g_echo = !on; g_isig = !on; g_icrnl = 1; g_vmin = 1;   // 0 = fully cooked defaults
    g_tail = g_head;
    irq_restore(fl);
}

struct bob_termios {
    uint32_t iflag, oflag, cflag, lflag;
    uint8_t cc[32];
};

void console_get_termios(struct bob_termios* t) {
    memset(t, 0, sizeof(*t));
    t->iflag = g_icrnl ? T_ICRNL : 0;
    t->oflag = 0x5;                     // OPOST | ONLCR (the screen always does this)
    t->cflag = 0xBF;                    // CS8 | CREAD | B38400-ish
    t->lflag = (g_raw ? 0 : T_ICANON) | (g_echo ? T_ECHO : 0) | (g_isig ? T_ISIG : 0);
    t->cc[T_VMIN] = (uint8_t)g_vmin;
    t->cc[3] = 0x7F;                    // VERASE ... (informational)
}

void console_set_termios(const struct bob_termios* t) {
    uint32_t fl = irq_save();
    int raw = (t->lflag & T_ICANON) ? 0 : 1;
    if (g_raw != 2 && raw != g_raw) { g_raw = raw; g_tail = g_head; }
    g_echo = (t->lflag & T_ECHO) != 0;
    g_isig = (t->lflag & T_ISIG) != 0;
    g_icrnl = (t->iflag & T_ICRNL) != 0;
    g_vmin = t->cc[T_VMIN];
    irq_restore(fl);
}

// irq context, raw mode: one byte straight into the ring
void console_push_byte(char c) {
    if (count() >= CON_BUF - 1) return;
    g_buf[g_head] = c; g_head = (g_head + 1) % CON_BUF;
    proc_wakeup(WAIT_CONSOLE);
}

// irq context
void console_push_line(const char* line) {
    uint32_t n = (uint32_t)strlen(line) + 1;          // + newline
    if (n > CON_BUF - 1 - count()) return;            // full: drop the line
    for (const char* p = line; *p; p++) { g_buf[g_head] = *p; g_head = (g_head + 1) % CON_BUF; }
    g_buf[g_head] = '\n'; g_head = (g_head + 1) % CON_BUF;
    proc_wakeup(WAIT_CONSOLE);
}

// blocks until at least one full line is available, returns up to max bytes
int console_read(char* dst, int max) {
    uint32_t fl = irq_save();
    while (count() == 0) {
        if (g_raw == 2 || (g_raw == 1 && g_vmin == 0)) { irq_restore(fl); return 0; }   // never blocks
        if (sig_interrupting()) { irq_restore(fl); return -1; }
        proc_sleep_on(WAIT_CONSOLE);
    }

    int n = 0;
    while (n < max && count() > 0) {
        char c = g_buf[g_tail];
        g_tail = (g_tail + 1) % CON_BUF;
        dst[n++] = c;
        if (c == '\n' && !g_raw) break;
    }
    irq_restore(fl);
    return n;
}
