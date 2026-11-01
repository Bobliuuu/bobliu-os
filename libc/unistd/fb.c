#include <fb.h>
#include <unistd.h>
#include <kernel/syscall.h>

int fb_init(struct fbinfo* out) {
    struct bob_fbinfo b;
    int r = __syscall3(SYS_fb_init, (int)&b, 0, 0);
    if (r < 0) return -1;
    out->width = b.width; out->height = b.height; out->pitch = b.pitch; out->bpp = b.bpp;
    out->addr = (unsigned*)b.addr;
    return 0;
}

int fb_exit(void) {
    return __syscall3(SYS_fb_exit, 0, 0, 0);
}
