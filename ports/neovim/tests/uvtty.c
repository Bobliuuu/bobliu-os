/* libuv tty input: raw mode, non-blocking stdin. Reports every read result for 6 seconds. */
#include <stdio.h>
#include <string.h>
#include <uv.h>
static uv_tty_t tin;
static uv_timer_t quit;
static int reads, eofs;
static void alloc_cb(uv_handle_t* h, size_t n, uv_buf_t* b) { (void)h; static char buf[64]; b->base = buf; b->len = n < sizeof buf ? n : sizeof buf; }
static void read_cb(uv_stream_t* s, ssize_t n, const uv_buf_t* b) {
    reads++;
    if (n == UV_EOF) { eofs++; printf("read_cb: EOF (#%d)\n", eofs); if (eofs > 3) uv_read_stop(s); return; }
    if (n < 0) { printf("read_cb: error %s\n", uv_strerror((int)n)); uv_read_stop(s); return; }
    printf("read_cb: %d byte(s), first=0x%02x\n", (int)n, (unsigned char)b->base[0]);
}
static void quit_cb(uv_timer_t* t) { (void)t; uv_tty_reset_mode(); uv_stop(uv_default_loop()); }
int main(void) {
    uv_loop_t* loop = uv_default_loop();
    int r = uv_tty_init(loop, &tin, 0, 1);
    printf("uv_tty_init(stdin)=%d\n", r);
    r = uv_tty_set_mode(&tin, UV_TTY_MODE_RAW);
    printf("set_mode raw=%d\n", r);
    uv_read_start((uv_stream_t*)&tin, alloc_cb, read_cb);
    uv_timer_init(loop, &quit); uv_timer_start(&quit, quit_cb, 6000, 0);
    printf("press a key within 6 s\n");
    uv_run(loop, UV_RUN_DEFAULT);
    printf("done: reads=%d eofs=%d\n", reads, eofs);
    return 0;
}
