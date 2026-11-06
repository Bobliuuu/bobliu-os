#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include <errno.h>
#include <sys/wait.h>
static int total, bad;
#define CHECK(name, cond) do { total++; if (!(cond)) { bad++; printf("FAIL %s\n", name); } else printf("ok   %s\n", name); } while (0)

static char out[128]; static int outn, exited, exit_status = -1;
static void alloc_cb(uv_handle_t* h, size_t n, uv_buf_t* b) { (void)h; static char buf[128]; b->base = buf; b->len = n < sizeof buf ? n : sizeof buf; }
static void read_cb(uv_stream_t* s, ssize_t n, const uv_buf_t* b) {
    if (n > 0 && outn + n < (ssize_t)sizeof out) { memcpy(out + outn, b->base, n); outn += (int)n; }
    else if (n < 0) uv_close((uv_handle_t*)s, 0);
}
static void exit_cb(uv_process_t* p, int64_t st, int sig) { (void)sig; exit_status = (int)st; exited = 1; uv_close((uv_handle_t*)p, 0); }

int main(void) {
    uv_loop_t* loop = uv_default_loop();
    printf("-- spawn\n");
    uv_process_t proc; uv_process_options_t o; memset(&o, 0, sizeof o);
    uv_pipe_t outp; uv_pipe_init(loop, &outp, 0);
    char* args[] = { "echo", "spawned by libuv", 0 };
    uv_stdio_container_t io[3];
    io[0].flags = UV_IGNORE;
    io[1].flags = (uv_stdio_flags)(UV_CREATE_PIPE | UV_WRITABLE_PIPE); io[1].data.stream = (uv_stream_t*)&outp;
    io[2].flags = UV_INHERIT_FD; io[2].data.fd = 2;
    o.file = "/bin/echo"; o.args = args; o.exit_cb = exit_cb; o.stdio_count = 3; o.stdio = io;
    int r = uv_spawn(loop, &proc, &o);
    printf("uv_spawn=%d %s pid=%d\n", r, r ? uv_strerror(r) : "", (int)proc.pid);
    CHECK("uv_spawn", r == 0);
    if (r == 0) {
        uv_read_start((uv_stream_t*)&outp, alloc_cb, read_cb);
        uv_run(loop, UV_RUN_DEFAULT);
        out[outn] = 0;
        printf("child said: %s", out);
        CHECK("child output", strstr(out, "spawned by libuv") != 0);
        CHECK("exit callback", exited && exit_status == 0);
    }
    printf("-- tty\n");
    uv_tty_t tty;
    r = uv_tty_init(loop, &tty, 1, 0);
    printf("uv_tty_init=%d %s\n", r, r ? uv_strerror(r) : "");
    CHECK("uv_tty_init", r == 0);
    if (r == 0) {
        int w = 0, h = 0;
        CHECK("tty winsize", uv_tty_get_winsize(&tty, &w, &h) == 0 && w == 80 && h == 25);
        CHECK("tty raw mode", uv_tty_set_mode(&tty, UV_TTY_MODE_RAW) == 0);
        uv_tty_reset_mode();
        uv_close((uv_handle_t*)&tty, 0);
        uv_run(loop, UV_RUN_DEFAULT);
    }
    CHECK("guess handle tty", uv_guess_handle(1) == UV_TTY);
    printf("uvtest2: %d/%d ok\n", total - bad, total);
    return bad != 0;
}
