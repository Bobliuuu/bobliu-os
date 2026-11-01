#pragma once
/* graphics: fb_init() switches the screen to 640x480, 32 bpp and maps the pixels.
   pixel (x,y) is ((unsigned*)addr)[y * (pitch/4) + x], colour 0x00RRGGBB.
   fb_exit() (or exiting) returns to text mode. needs qemu -vga std (the default). */
struct fbinfo { unsigned width, height, pitch, bpp; unsigned* addr; };
int fb_init(struct fbinfo* out);
int fb_exit(void);
/* tcsetraw(2) gives raw set-1 scancodes: bit 7 set = key released, 0xE0 prefixes extended keys */
