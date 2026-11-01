// doomgeneric platform layer for bobliu: framebuffer out, scancode keys in, ms clock
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fb.h>

#include "doomgeneric.h"
#include "doomkeys.h"

static struct fbinfo fb;
static int yoff;
static int g_fps;     // "doom -fps": average frames/second is printed when you quit
static void print_fps(void);

void DG_Init(void) {
    if (fb_init(&fb) < 0) {
        printf("doom: no framebuffer (run qemu with -vga std)\n");
        exit(1);
    }
    yoff = ((int)fb.height - DOOMGENERIC_RESY) / 2;
    if (yoff < 0) yoff = 0;
    memset(fb.addr, 0, fb.pitch * fb.height);   // the framebuffer still holds text-mode leftovers
    tcsetraw(2);                          // raw scancodes with key up/down
    if (g_fps) atexit(print_fps);         // runs after fb_exit (atexit is LIFO), so it lands on the text screen
    atexit((void (*)(void))fb_exit);
}

static uint32_t g_frames, g_t0;

void DG_DrawFrame(void) {
    if (!g_frames) g_t0 = DG_GetTicksMs();
    g_frames++;
    for (int y = 0; y < DOOMGENERIC_RESY && y + yoff < (int)fb.height; y++)
        memcpy(fb.addr + (y + yoff) * (fb.pitch / 4), DG_ScreenBuffer + y * DOOMGENERIC_RESX,
               DOOMGENERIC_RESX * 4);
}

static void print_fps(void) {
    uint32_t ms = DG_GetTicksMs() - g_t0;
    if (ms) printf("doom: %u frames in %u ms = %u fps\n", g_frames, ms, g_frames * 1000 / ms);
}

void DG_SleepMs(uint32_t ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000 };
    nanosleep(&ts, 0);
}

uint32_t DG_GetTicksMs(void);
uint32_t DG_GetTicksMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000u + ts.tv_nsec / 1000000u;
}

// set 1 scancode -> doom key
static unsigned char map_key(int sc, int ext) {
    if (ext) {
        switch (sc) {
        case 0x48: return KEY_UPARROW;
        case 0x50: return KEY_DOWNARROW;
        case 0x4B: return KEY_LEFTARROW;
        case 0x4D: return KEY_RIGHTARROW;
        case 0x1D: return KEY_FIRE;          // right ctrl
        case 0x38: return KEY_LALT;          // right alt: strafe
        case 0x1C: return KEY_ENTER;
        case 0x47: return KEY_HOME;
        case 0x4F: return KEY_END;
        case 0x49: return KEY_PGUP;
        case 0x51: return KEY_PGDN;
        case 0x53: return KEY_DEL;
        }
        return 0;
    }
    switch (sc) {
    case 0x01: return KEY_ESCAPE;
    case 0x1C: return KEY_ENTER;
    case 0x0F: return KEY_TAB;
    case 0x0E: return KEY_BACKSPACE;
    case 0x39: return KEY_USE;               // space
    case 0x1D: return KEY_FIRE;              // left ctrl
    case 0x38: return KEY_LALT;              // left alt: strafe
    case 0x2A: case 0x36: return KEY_RSHIFT; // shift: run
    case 0x33: return KEY_STRAFE_L;          // ,
    case 0x34: return KEY_STRAFE_R;          // .
    case 0x0C: return KEY_MINUS;
    case 0x0D: return KEY_EQUALS;
    }
    if (sc >= 0x3B && sc <= 0x44) return KEY_F1 + (sc - 0x3B);
    if (sc == 0x57) return KEY_F11;
    if (sc == 0x58) return KEY_F12;
    static const char digits[] = "1234567890";
    if (sc >= 0x02 && sc <= 0x0B) return digits[sc - 2];
    static const char rows[3][11] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    static const unsigned char start[3] = { 0x10, 0x1E, 0x2C };
    for (int r = 0; r < 3; r++)
        if (sc >= start[r] && sc < start[r] + (int)strlen(rows[r])) return rows[r][sc - start[r]];
    return 0;
}

int DG_GetKey(int* pressed, unsigned char* key) {
    static int ext;
    unsigned char c;
    while (read(0, &c, 1) == 1) {
        if (c == 0xE0) { ext = 1; continue; }
        int down = !(c & 0x80);
        unsigned char k = map_key(c & 0x7F, ext);
        ext = 0;
        if (!k) continue;
        *pressed = down;
        *key = k;
        return 1;
    }
    return 0;
}

void DG_SetWindowTitle(const char* title) { (void)title; }

int main(int argc, char** argv) {
    // defaults first, so anything typed after "doom" (e.g. -skill 2 -warp 1 -fps, or another -iwad) wins
    static char* av[32] = { "doom", "-iwad", "/share/freedoom1.wad", "-savedir", "/tmp" };
    int n = 5;
    for (int i = 1; i < argc && n < 31; i++) {
        av[n++] = argv[i];
        if (!strcmp(argv[i], "-fps")) g_fps = 1;
    }
    av[n] = 0;
    dup2(1, 2);                           // I_Error goes to stderr: follow stdout so "doom > log" catches it
    doomgeneric_Create(n, av);
    for (;;) doomgeneric_Tick();
    return 0;
}
