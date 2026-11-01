// graphics + key-state test: arrows move the square (held keys work), esc quits
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fb.h>

static struct fbinfo fb;

static void rect(int x, int y, int w, int h, unsigned c) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= (int)fb.height) continue;
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < (int)fb.width) fb.addr[j * (fb.pitch / 4) + i] = c;
    }
}

int main(void) {
    if (fb_init(&fb) < 0) { printf("fbtest: no framebuffer (need qemu -vga std)\n"); return 1; }
    for (int y = 0; y < (int)fb.height; y++)
        for (int x = 0; x < (int)fb.width; x++)
            fb.addr[y * (fb.pitch / 4) + x] = ((x * 255 / fb.width) << 16) | ((y * 255 / fb.height) << 8) | 0x40;

    tcsetraw(2);                 // scancodes, read() returns 0 when no key event is pending
    int x = 300, y = 220, left = 0, right = 0, up = 0, down = 0, ext = 0, frames = 0, quit = 0;
    while (!quit) {
        unsigned char c;
        while (read(0, &c, 1) == 1) {
            if (c == 0xE0) { ext = 1; continue; }
            int pressed = !(c & 0x80), k = c & 0x7F;      // pressed is really "pressed" here
            if (!ext && k == 1 && pressed) quit = 1;
            if (ext) {
                if (k == 0x4B) left = pressed; else if (k == 0x4D) right = pressed;
                else if (k == 0x48) up = pressed; else if (k == 0x50) down = pressed;
            }
            ext = 0;
        }
        rect(x, y, 40, 40, 0x204080);
        x += (right - left) * 4; y += (down - up) * 4;
        rect(x, y, 40, 40, 0xFFFFFF);
        frames++;
        struct timespec ts = { 0, 16000000 };
        nanosleep(&ts, 0);
    }
    tcsetraw(0);
    fb_exit();
    printf("fbtest: ok (%d frames)\n", frames);
    return 0;
}
