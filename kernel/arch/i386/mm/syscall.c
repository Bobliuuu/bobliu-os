#include <stdint.h>
#include <string.h>
#include <arch/i386/isr.h>
#include <arch/i386/paging.h>
#include <kernel/syscall.h>
#include <kernel/proc.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>
#include <kernel/pipe.h>
#include <kernel/signal.h>
#include <arch/i386/timer.h>
#include <arch/i386/irq.h>
#include <arch/i386/fb.h>

int console_read(char* dst, int max);
void console_set_raw(int on);
void console_get_termios(void* t);
void console_set_termios(const void* t);

extern int putchar(int);

// user buffer must sit entirely inside the user region
static int user_range_ok(uint32_t p, uint32_t len) {
    if (p < USER_LO || p >= USER_HI) return 0;
    if (len > USER_HI - p) return 0;
    // every page has to be mapped, or the kernel would fault touching it
    page_directory_t pd = proc_current()->pd;
    uint32_t last = p + (len ? len - 1 : 0);
    for (uint32_t va = p & ~0xFFFu; va <= (last & ~0xFFFu); va += PAGE_SIZE) {
        if (!paging_translate_in(pd, va)) return 0;
    }
    return 1;
}

// copy a NUL-terminated string out of user space (bounded, validated)
static int copy_user_str(char* dst, uint32_t uptr, uint32_t max) {
    for (uint32_t i = 0; i < max; i++) {
        if (!user_range_ok(uptr + i, 1)) return -1;
        char c = ((const char*)uptr)[i];
        dst[i] = c;
        if (!c) return 0;
    }
    return -1;
}

// make `in` absolute against the caller's cwd, collapsing //, . and ..
static int abs_path(const char* in, char* out, uint32_t max) {
    char tmp[2 * PROC_ARG_LEN];
    uint32_t n = 0;
    if (in[0] != '/') {
        const char* cwd = proc_current()->group->cwd;
        for (; *cwd && n < sizeof(tmp) - 1; ) tmp[n++] = *cwd++;
        if (n && tmp[n - 1] != '/' && n < sizeof(tmp) - 1) tmp[n++] = '/';
    }
    for (; *in && n < sizeof(tmp) - 1; ) tmp[n++] = *in++;
    tmp[n] = '\0';

    uint32_t o = 0;
    out[o++] = '/';
    const char* p = tmp;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char* c = p;
        while (*p && *p != '/') p++;
        uint32_t len = (uint32_t)(p - c);
        if (len == 1 && c[0] == '.') continue;
        if (len == 2 && c[0] == '.' && c[1] == '.') {
            if (o > 1) {                    // drop the last component
                while (out[o - 1] != '/') o--;
                if (o > 1) o--;
            }
            continue;
        }
        if (o > 1) { if (o + 1 >= max) return -1; out[o++] = '/'; }
        if (o + len >= max) return -1;
        for (uint32_t i = 0; i < len; i++) out[o++] = c[i];
    }
    out[o] = '\0';
    return 0;
}

static int copy_user_path(char* dst, uint32_t uptr) {
    char raw[PROC_ARG_LEN];
    if (copy_user_str(raw, uptr, sizeof(raw)) < 0) return -1;
    return abs_path(raw, dst, PROC_ARG_LEN);
}

static int sys_write(uint32_t fd, const char* buf, uint32_t len) {
    int e = proc_fd_get((int)fd);
    if (e == -1) return -1;
    if (!user_range_ok((uint32_t)buf, len)) return -1;
    if (e >= 0) return vfs_write(e, buf, len);
    for (uint32_t i = 0; i < len; i++) putchar((unsigned char)buf[i]);
    return (int)len;
}

static void syscall_dispatch(regs_t* r) {
    proc_current()->tf = r;

    switch (r->eax) {
    case SYS_exit:
        proc_exit((int)r->ebx);
    case SYS_fork:
        r->eax = (uint32_t)proc_fork(r);
        return;
    case SYS_write:
        r->eax = (uint32_t)sys_write(r->ebx, (const char*)r->ecx, r->edx);
        return;
    case SYS_waitpid: {
        int status = 0;
        int ret = proc_wait((int)r->ebx, &status, (int)r->edx);
        if (ret > 0 && r->ecx) {
            if (user_range_ok(r->ecx, 4)) *(int*)r->ecx = status;
            else ret = -1;
        }
        r->eax = (uint32_t)ret;
        return;
    }
    case SYS_read: {
        if (!user_range_ok(r->ecx, r->edx)) { r->eax = (uint32_t)-1; return; }
        int e = proc_fd_get((int)r->ebx);
        if (e == FD_CONSOLE)
            r->eax = (uint32_t)console_read((char*)r->ecx, (int)r->edx);
        else
            r->eax = (uint32_t)(e < 0 ? -1 : vfs_read(e, (void*)r->ecx, r->edx));
        return;
    }
    case SYS_open: {
        char path[PROC_ARG_LEN];
        if (copy_user_path(path, r->ebx) < 0) { r->eax = (uint32_t)-1; return; }
        int vfd = vfs_open_flags(path, (int)r->ecx);
        if (vfd < 0) { r->eax = (uint32_t)-1; return; }
        int ufd = proc_fd_alloc(vfd);
        if (ufd < 0) vfs_close(vfd);
        r->eax = (uint32_t)ufd;
        return;
    }
    case SYS_close: {
        r->eax = (uint32_t)proc_fd_close((int)r->ebx);
        return;
    }
    case SYS_pipe: {
        vnode_t *rv, *wv;
        if (!user_range_ok(r->ebx, 8) || pipe_create(&rv, &wv) < 0) { r->eax = (uint32_t)-1; return; }
        int rf = vfs_open_vnode(rv, O_RDONLY);
        int wf = rf < 0 ? -1 : vfs_open_vnode(wv, O_WRONLY);
        int ru = wf < 0 ? -1 : proc_fd_alloc(rf);
        int wu = ru < 0 ? -1 : proc_fd_alloc(wf);
        if (wu < 0) {                       // unwind whatever succeeded
            if (ru >= 0) proc_fd_close(ru);
            else if (wf >= 0) vfs_close(rf);
            if (wf >= 0) vfs_close(wf);
            r->eax = (uint32_t)-1; return;
        }
        ((int*)r->ebx)[0] = ru;
        ((int*)r->ebx)[1] = wu;
        r->eax = 0;
        return;
    }
    case SYS_fb_init: {
        uint32_t info[5];
        if (!user_range_ok(r->ebx, sizeof(info)) || fb_acquire(proc_current()->group, info) < 0) { r->eax = (uint32_t)-1; return; }
        for (int i = 0; i < 5; i++) ((uint32_t*)r->ebx)[i] = info[i];
        r->eax = 0;
        return;
    }
    case SYS_fb_exit:
        fb_release(proc_current()->group);
        r->eax = 0;
        return;
    case SYS_tcsetraw:
        console_set_raw((int)r->ebx);
        r->eax = 0;
        return;
    case SYS_tcgetattr:
    case SYS_tcsetattr: {
        if (proc_fd_get((int)r->ebx) != FD_CONSOLE || !user_range_ok(r->ecx, 48)) { r->eax = (uint32_t)-1; return; }
        if (r->eax == SYS_tcgetattr) console_get_termios((void*)r->ecx);
        else console_set_termios((void*)r->ecx);
        r->eax = 0;
        return;
    }
    case SYS_dup:
        r->eax = (uint32_t)proc_fd_dup((int)r->ebx, -1);
        return;
    case SYS_dup2:
        r->eax = (uint32_t)((int)r->ecx < 0 ? -1 : proc_fd_dup((int)r->ebx, (int)r->ecx));
        return;
    case SYS_unlink:
    case SYS_mkdir: {
        char path[PROC_ARG_LEN];
        if (copy_user_path(path, r->ebx) < 0) { r->eax = (uint32_t)-1; return; }
        r->eax = (uint32_t)(r->eax == SYS_unlink ? vfs_unlink(path) : vfs_mkdir(path));
        return;
    }
    case SYS_lseek: {
        int vfd = proc_fd_get((int)r->ebx);
        r->eax = (uint32_t)(vfd < 0 ? -1 : vfs_seek(vfd, (int32_t)r->ecx, (int)r->edx));  // console fd -> -1
        return;
    }
    case SYS_stat: {
        char path[PROC_ARG_LEN];
        vfs_stat_t st;
        if (copy_user_path(path, r->ebx) < 0 || !user_range_ok(r->ecx, sizeof(struct bob_stat)) ||
            vfs_stat(path, &st) < 0) { r->eax = (uint32_t)-1; return; }
        struct bob_stat* out = (struct bob_stat*)r->ecx;
        out->size = st.size;
        out->is_dir = st.is_dir;
        r->eax = 0;
        return;
    }
    case SYS_readdir: {
        char path[PROC_ARG_LEN];
        char name[128];
        if (copy_user_path(path, r->ebx) < 0 || !user_range_ok(r->edx, sizeof(name))) {
            r->eax = (uint32_t)-1; return;
        }
        int rc = vfs_readdir_path(path, r->ecx, name, sizeof(name));
        if (rc > 0) memcpy((void*)r->edx, name, sizeof(name));
        r->eax = (uint32_t)rc;
        return;
    }
    case SYS_execve: {
        // argv/envp strings are copied out of the old image into a kernel buffer first
        char path[PROC_ARG_LEN];
        struct exec_buf { char args[PROC_MAX_ARGS][PROC_ARG_LEN]; char env[PROC_MAX_ENV][PROC_ENV_LEN]; };
        char* argv[PROC_MAX_ARGS];
        char* envp[PROC_MAX_ENV];
        int argc = 0, envc = 0;
        if (copy_user_path(path, r->ebx) < 0) { r->eax = (uint32_t)-1; return; }
        struct exec_buf* eb = (struct exec_buf*)kmalloc(sizeof(*eb));
        if (!eb) { r->eax = (uint32_t)-1; return; }

        if (r->ecx) {
            for (; argc < PROC_MAX_ARGS; argc++) {
                if (!user_range_ok(r->ecx + 4u * (uint32_t)argc, 4)) goto exec_fail;
                uint32_t up = ((uint32_t*)r->ecx)[argc];
                if (!up) break;
                if (copy_user_str(eb->args[argc], up, PROC_ARG_LEN) < 0) goto exec_fail;
                argv[argc] = eb->args[argc];
            }
        }
        if (r->edx) {
            for (; envc < PROC_MAX_ENV; envc++) {
                if (!user_range_ok(r->edx + 4u * (uint32_t)envc, 4)) goto exec_fail;
                uint32_t up = ((uint32_t*)r->edx)[envc];
                if (!up) break;
                if (copy_user_str(eb->env[envc], up, PROC_ENV_LEN) < 0) goto exec_fail;
                envp[envc] = eb->env[envc];
            }
        }
        if (argc == 0) { memcpy(eb->args[0], path, sizeof(path)); argv[0] = eb->args[0]; argc = 1; }

        // on success proc_exec rewrites the trap frame (eip/esp) and eax stays 0
        r->eax = (uint32_t)proc_exec(path, argv, argc, envp, envc, r);
        kfree(eb);
        return;
exec_fail:
        kfree(eb);
        r->eax = (uint32_t)-1;
        return;
    }
    case SYS_brk:
        r->eax = (uint32_t)proc_brk(r->ebx);
        return;
    case SYS_chdir: {
        char path[PROC_ARG_LEN];
        vfs_stat_t st;
        if (copy_user_path(path, r->ebx) < 0 || vfs_stat(path, &st) < 0 || !st.is_dir) { r->eax = (uint32_t)-1; return; }
        strcpy(proc_current()->group->cwd, path);
        r->eax = 0;
        return;
    }
    case SYS_getcwd: {
        const char* cwd = proc_current()->group->cwd;
        uint32_t n = strlen(cwd) + 1;
        if (n > r->ecx || !user_range_ok(r->ebx, n)) { r->eax = (uint32_t)-1; return; }
        memcpy((void*)r->ebx, cwd, n);
        r->eax = n;
        return;
    }
    case SYS_kill: {
        proc_t* t = proc_by_pid((int)r->ebx);
        r->eax = (uint32_t)(t ? sig_send(t, (int)r->ecx) : -1);
        return;
    }
    case SYS_signal:
        r->eax = (uint32_t)sig_set((int)r->ebx, r->ecx, r->edx);
        return;
    case SYS_sigreturn:
        sig_return(r);
        return;
    case SYS_time: {
        uint32_t t = wall_time();
        if (r->ebx) {
            if (!user_range_ok(r->ebx, 4)) { r->eax = (uint32_t)-1; return; }
            *(uint32_t*)r->ebx = t;
        }
        r->eax = t;
        return;
    }
    case SYS_clock_gettime: {
        struct bob_timespec* ts = (struct bob_timespec*)r->ecx;
        if (r->ebx > 1 || !user_range_ok(r->ecx, sizeof(*ts))) { r->eax = (uint32_t)-1; return; }
        uint64_t ms = timer_ms();
        if (r->ebx == 0) { ts->tv_sec = (int)(wall_time()); ts->tv_nsec = (int)((ms % 1000) * 1000000); }
        else             { ts->tv_sec = (int)(ms / 1000);   ts->tv_nsec = (int)((ms % 1000) * 1000000); }
        r->eax = 0;
        return;
    }
    case SYS_nanosleep: {
        struct bob_timespec* ts = (struct bob_timespec*)r->ebx;
        if (!user_range_ok(r->ebx, sizeof(*ts)) || ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000) {
            r->eax = (uint32_t)-1; return;
        }
        uint32_t ticks = (uint32_t)ts->tv_sec * TIMER_HZ + ((uint32_t)ts->tv_nsec + 9999999u) / 10000000u;
        uint32_t target = timer_ticks() + ticks;
        int rc = 0;
        uint32_t fl = irq_save();
        while ((int32_t)(timer_ticks() - target) < 0) {
            if (sig_interrupting()) { rc = -1; break; }
            proc_sleep_on(WAIT_TIMER);
        }
        irq_restore(fl);
        r->eax = (uint32_t)rc;
        return;
    }
    case SYS_clone: {                       // ebx -> struct bob_clone {fn, arg, stack_top, exit_stub}
        struct bob_clone* a = (struct bob_clone*)r->ebx;
        if (!user_range_ok(r->ebx, sizeof(*a))) { r->eax = (uint32_t)-1; return; }
        uint32_t sp = a->stack_top & ~3u;
        if (!user_range_ok(sp - 8, 8) || a->fn < USER_LO || a->fn >= USER_HI) { r->eax = (uint32_t)-1; return; }
        ((uint32_t*)sp)[-1] = a->arg;        // cdecl: fn(arg) returns into exit_stub
        ((uint32_t*)sp)[-2] = a->exit_stub;
        r->eax = (uint32_t)proc_thread_create(a->fn, sp - 8);
        return;
    }
    case SYS_thread_exit:
        proc_thread_exit((int)r->ebx);
    case SYS_thread_join: {
        int code = 0;
        int rc = proc_thread_join((int)r->ebx, &code);
        if (rc > 0 && r->ecx) {
            if (user_range_ok(r->ecx, 4)) *(int*)r->ecx = code; else rc = -1;
        }
        r->eax = (uint32_t)rc;
        return;
    }
    case SYS_gettid:
        r->eax = (uint32_t)proc_current()->pid;
        return;
    case SYS_sched_yield:
        proc_yield();
        r->eax = 0;
        return;
    case SYS_futex: {                       // ebx = addr, ecx = op (0 wait, 1 wake), edx = value
        if (!user_range_ok(r->ebx, 4) || (r->ebx & 3) || r->ecx > 1) { r->eax = (uint32_t)-1; return; }
        int chan = FUTEX_CHAN + (int)((r->ebx >> 2) & 63);
        if (r->ecx == 1) { proc_wakeup(chan); r->eax = 0; return; }
        uint32_t fl = irq_save();
        int rc = 0;
        if (*(volatile int*)r->ebx == (int)r->edx) {      // still the value we were told: sleep
            if (sig_interrupting()) rc = -1;
            else proc_sleep_on(chan);
        }
        irq_restore(fl);
        r->eax = (uint32_t)rc;
        return;
    }
    case SYS_getpid:
        r->eax = (uint32_t)proc_current()->pid;
        return;
    default:
        r->eax = (uint32_t)-1;
        return;
    }
}

void syscall_handle(regs_t* r) {
    syscall_dispatch(r);
    if (r->cs & 3) sig_deliver(r);       // about to return to ring 3
}
