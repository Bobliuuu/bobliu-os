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

int console_is_raw(void) { return g_raw; }

// raw mode: keys arrive as bytes with no echo / line editing; flushes pending input on every switch
void console_set_raw(int on) {
    uint32_t fl = irq_save();
    g_raw = on ? 1 : 0;
    g_tail = g_head;
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
