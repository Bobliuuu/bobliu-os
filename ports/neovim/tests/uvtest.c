#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

static int total, bad;
#define CHECK(name, cond) do { total++; if (!(cond)) { bad++; printf("FAIL %s\n", name); } else printf("ok   %s\n", name); } while (0)
#define STAGE(n) printf("-- %s\n", n)

static int timer_hits;
static void on_timer(uv_timer_t* t) { if (++timer_hits == 3) uv_timer_stop(t); }

static void stop_cb(uv_timer_t* t) { uv_stop(t->loop); }
static uv_pipe_t rp;
static char rbuf[64];
static int got_bytes;
static void alloc_cb(uv_handle_t* h, size_t n, uv_buf_t* b) { (void)h; b->base = rbuf; b->len = sizeof rbuf < n ? sizeof rbuf : n; }
static void read_cb(uv_stream_t* s, ssize_t n, const uv_buf_t* b) {
    printf("read_cb n=%d (%s)\n", (int)n, n < 0 ? uv_strerror((int)n) : "data");
    if (n > 0) got_bytes += (int)n;
    else if (n < 0) uv_close((uv_handle_t*)s, 0);
    (void)b;
}
static void write_cb(uv_write_t* w, int status) { (void)w; printf("write_cb status=%d (%s)\n", status, status ? uv_strerror(status) : "ok"); }

static int async_hits;
static void async_cb(uv_async_t* a) { async_hits++; uv_close((uv_handle_t*)a, 0); }
static void* async_thread(void* p) { usleep(20000); uv_async_send((uv_async_t*)p); return 0; }

static int fs_done;
static void fs_cb(uv_fs_t* r) { fs_done = (int)r->result; }

int main(void) {
    uv_loop_t* loop = uv_default_loop();
    CHECK("loop init", loop != NULL);
    printf("libuv %s\n", uv_version_string());

    STAGE("timer");
    /* timer: 3 hits at 10 ms */
    uv_timer_t t; uv_timer_init(loop, &t); uv_timer_start(&t, on_timer, 10, 10);
    uv_run(loop, UV_RUN_DEFAULT);
    CHECK("timer x3", timer_hits == 3);

    STAGE("pipe");
    /* pipe pair through uv_pipe_open + stream read/write */
    int fds[2];
    CHECK("pipe()", pipe(fds) == 0);
    uv_pipe_init(loop, &rp, 0);
    CHECK("pipe_open r", uv_pipe_open(&rp, fds[0]) == 0);
    uv_pipe_t wp; uv_pipe_init(loop, &wp, 0);
    CHECK("pipe_open w", uv_pipe_open(&wp, fds[1]) == 0);
    uv_read_start((uv_stream_t*)&rp, alloc_cb, read_cb);
    uv_write_t w; uv_buf_t b = uv_buf_init("hello uv", 8);
    uv_write(&w, (uv_stream_t*)&wp, &b, 1, write_cb);
    uv_timer_t stop; uv_timer_init(loop, &stop);
    uv_timer_start(&stop, stop_cb, 100, 0);
    uv_run(loop, UV_RUN_DEFAULT);
    printf("got_bytes=%d\n", got_bytes);
    CHECK("pipe read", got_bytes == 8 && memcmp(rbuf, "hello uv", 8) == 0);
    uv_close((uv_handle_t*)&rp, 0); uv_close((uv_handle_t*)&wp, 0); uv_close((uv_handle_t*)&stop, 0);
    uv_run(loop, UV_RUN_DEFAULT);

    STAGE("async");
    /* async wakeup from another thread */
    uv_async_t a; uv_async_init(loop, &a, async_cb);
    uv_thread_t th; uv_thread_create(&th, (uv_thread_cb)async_thread, &a);
    uv_run(loop, UV_RUN_DEFAULT);
    uv_thread_join(&th);
    CHECK("async from thread", async_hits == 1);

    STAGE("fs");
    /* fs via the thread pool */
    uv_fs_t fr;
    uv_fs_open(loop, &fr, "/hello.txt", O_RDONLY, 0, fs_cb);
    uv_run(loop, UV_RUN_DEFAULT);
    CHECK("fs_open async", fs_done >= 0);
    uv_fs_req_cleanup(&fr);
    uv_fs_t st; CHECK("fs_stat sync", uv_fs_stat(loop, &st, "/hello.txt", 0) == 0 && st.statbuf.st_size > 0);
    uv_fs_req_cleanup(&st);

    CHECK("hrtime", uv_hrtime() > 0);
    uv_loop_close(loop);
    printf("uvtest: %d/%d ok\n", total - bad, total);
    return bad != 0;
}
