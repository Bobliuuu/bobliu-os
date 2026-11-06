// anonymous pipes: a 4 KiB ring buffer with a read-end vnode and a write-end vnode.
// readers sleep while it is empty and a writer still exists, writers while it is full.
#include <kernel/pipe.h>
#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/heap.h>
#include <arch/i386/irq.h>
#include <string.h>

#define PIPE_BUF 4096
#define PIPE_CHAN_BASE 0x1000

typedef struct pipe {
    uint8_t buf[PIPE_BUF];
    uint32_t head, count;       // read position, bytes buffered
    int readers_open, writers_open;
    int id;
    vnode_t rd, wr;
} pipe_t;

static const vnode_ops_t g_rd_ops, g_wr_ops;
static int g_next_id = 0;

static pipe_t* pipe_of(vnode_t* vn) { return (pipe_t*)vn->fs_data; }

int pipe_create(vnode_t** rd, vnode_t** wr) {
    pipe_t* p = (pipe_t*)kmalloc(sizeof(pipe_t));
    if (!p) return -1;
    memset(p, 0, sizeof(*p));
    p->id = g_next_id++;
    p->readers_open = p->writers_open = 1;
    p->rd.ops = &g_rd_ops; p->rd.fs_data = p; p->rd.kind = VK_FIFO;
    p->wr.ops = &g_wr_ops; p->wr.fs_data = p; p->wr.kind = VK_FIFO;
    *rd = &p->rd;
    *wr = &p->wr;
    return 0;
}

static int chan(pipe_t* p) { return PIPE_CHAN_BASE + p->id; }

static int pipe_read(vnode_t* vn, uint32_t off, void* buf, uint32_t len) {
    (void)off;
    pipe_t* p = pipe_of(vn);
    if (len == 0) return 0;

    uint32_t fl = irq_save();
    while (p->count == 0) {
        if (!p->writers_open) { irq_restore(fl); return 0; }      // EOF
        if (proc_current()->io_nb) { irq_restore(fl); return -11; }   // EAGAIN
        if (sig_interrupting()) { irq_restore(fl); return -4; }       // EINTR
        proc_sleep_on(chan(p));
    }
    uint32_t n = len < p->count ? len : p->count;
    uint8_t* out = (uint8_t*)buf;
    for (uint32_t i = 0; i < n; i++) out[i] = p->buf[(p->head + i) % PIPE_BUF];
    p->head = (p->head + n) % PIPE_BUF;
    p->count -= n;
    proc_wakeup(chan(p));
    proc_wakeup(WAIT_POLL);
    irq_restore(fl);
    return (int)n;
}

static int pipe_write(vnode_t* vn, uint32_t off, const void* buf, uint32_t len) {
    (void)off;
    pipe_t* p = pipe_of(vn);
    const uint8_t* in = (const uint8_t*)buf;
    uint32_t done = 0;

    uint32_t fl = irq_save();
    while (done < len) {
        if (!p->readers_open) { irq_restore(fl); return done ? (int)done : -1; }   // broken pipe
        if (p->count == PIPE_BUF) {
            if (proc_current()->io_nb) { irq_restore(fl); return done ? (int)done : -11; }
            if (sig_interrupting()) { irq_restore(fl); return done ? (int)done : -4; }
            proc_sleep_on(chan(p));
            continue;
        }
        uint32_t n = PIPE_BUF - p->count;
        if (n > len - done) n = len - done;
        for (uint32_t i = 0; i < n; i++) p->buf[(p->head + p->count + i) % PIPE_BUF] = in[done + i];
        p->count += n;
        done += n;
        proc_wakeup(chan(p));
        proc_wakeup(WAIT_POLL);
    }
    irq_restore(fl);
    return (int)done;
}

// last open file on an end went away
static void pipe_put(vnode_t* vn) {
    pipe_t* p = pipe_of(vn);
    uint32_t fl = irq_save();
    if (vn == &p->rd) p->readers_open = 0; else p->writers_open = 0;
    proc_wakeup(chan(p));                  // blocked peers re-check (EOF / EPIPE)
    proc_wakeup(WAIT_POLL);
    int dead = !p->readers_open && !p->writers_open;
    irq_restore(fl);
    if (dead) kfree(p);
}

static int pipe_poll(vnode_t* vn, int events) {
    pipe_t* p = pipe_of(vn);
    int r = 0;
    if (vn == &p->rd) {
        if (p->count > 0) r |= 1;
        if (!p->writers_open) r |= 16 | (events & 1);    // EOF: readable (read returns 0) and hung up
    } else {
        if (p->count < PIPE_BUF) r |= 4;
        if (!p->readers_open) r |= 8;
    }
    return r;
}

static const vnode_ops_t g_rd_ops = { .read = pipe_read, .put = pipe_put, .poll = pipe_poll };
static const vnode_ops_t g_wr_ops = { .write = pipe_write, .put = pipe_put, .poll = pipe_poll };
