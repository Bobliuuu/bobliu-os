// arch/i386/dev/keyboard.c
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <arch/i386/portio.h>
#include <arch/i386/isr.h>     // for regs_t and handler typedef (whatever yours is)
#include <kernel/tty.h>        // terminal_putchar / terminal_write / etc (from your meaty skeleton)
#include <kernel/shell.h>

// ===== Adjust these if your project uses different names =====
// After PIC remap(0x20,0x28): IRQ1 is vector 0x21 (33)
#define IRQ1_VECTOR 33

// If your project uses a different registration function name,
// rename this call in keyboard_init() accordingly.
extern void register_interrupt_handler(uint8_t n, void (*handler)(regs_t*));

// Optional: if you already have this in tty.c, keep it.
// If not, either implement it in tty.c or replace with a fallback below.
extern void terminal_backspace(void);

// ===== PS/2 Set 1 scancodes (common in QEMU) =====
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPS     0x3A
#define SC_ENTER    0x1C
#define SC_BACKSP   0x0E

// simple input buffer
#define KBD_LINE_MAX 128
static char linebuf[KBD_LINE_MAX];
static int  line_len = 0;

static volatile bool g_shift = false;
static volatile bool g_caps  = false;

static bool shell_enabled = false;

// Basic US keymap (set 1). Index = scancode (0..127).
// Only includes printable keys you care about right now.
static const char keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' ',
};

static const char keymap_shift[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L',
    [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M',
    [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x39] = ' ',
};

// Line buffer commands
static void line_reset(void) {
    line_len = 0;
    linebuf[0] = '\0';
}

static void line_push(char c) {
    if (line_len >= KBD_LINE_MAX - 1) return;
    linebuf[line_len++] = c;
    linebuf[line_len] = '\0';
}

static void line_pop(void) {
    if (line_len <= 0) return;
    line_len--;
    linebuf[line_len] = '\0';
}

// ASCII casing helper for letters only
static inline char apply_caps_shift(char c, bool shift, bool caps) {
    if (c >= 'a' && c <= 'z') {
        bool upper = shift ^ caps;
        if (upper) return (char)(c - 'a' + 'A');
        return c;
    }
    if (c >= 'A' && c <= 'Z') {
        bool upper = shift ^ caps;
        if (!upper) return (char)(c - 'A' + 'a');
        return c;
    }
    return c;
}

static void keyboard_irq(regs_t* r) {
    (void)r;

    // Read scancode
    uint8_t sc = inb(0x60);

    // Handle 0xE0 extended prefix: for now, ignore next byte
    // (arrow keys, etc.). This keeps things simple and prevents weirdness.
    static bool e0 = false;
    if (sc == 0xE0) {
        e0 = true;
        return;
    }
    if (e0) {
        // ignore extended scancode for now
        e0 = false;
        return;
    }

    bool released = (sc & 0x80) != 0;
    sc &= 0x7F;

    // Modifier keys
    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        g_shift = !released;
        return;
    }
    if (sc == SC_CAPS) {
        if (!released) g_caps = !g_caps;
        return;
    }

    // Only act on key press (make), ignore releases for normal keys
    if (released) return;

    // Special keys
    if (sc == SC_ENTER) {
        terminal_putchar('\n');
        if (shell_enabled) {
            shell_on_line(linebuf);
        }
        line_reset();
        return;
    }
    if (sc == SC_BACKSP) {
        if (line_len > 0) {
            line_pop();
            terminal_backspace();
        }
        return;
    }

    // Map to character
    char c = g_shift ? keymap_shift[sc] : keymap[sc];
    if (c == 0) return;

    // Apply caps/shift logic for letters
    c = apply_caps_shift(c, g_shift, g_caps);

    terminal_putchar(c);
    line_push(c);
}

void keyboard_init(void) {
    // vector 33 after PIC remap(0x20,0x28)
    // Doesnt matter since it is automapped in irq
    irq_install_handler(1, keyboard_irq);
}

void keyboard_enable_shell(bool on) {
    shell_enabled = on;
}