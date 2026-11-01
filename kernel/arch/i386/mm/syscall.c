#include <stdint.h>
#include <string.h>
#include <arch/i386/isr.h>
#include <arch/i386/paging.h>
#include <kernel/syscall.h>
#include <kernel/proc.h>
#include <kernel/vfs.h>

int console_read(char* dst, int max);

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
        const char* cwd = proc_current()->cwd;
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

void syscall_handle(regs_t* r) {
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
        int ret = proc_wait((int)r->ebx, &status);
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
        char path[PROC_ARG_LEN];
        char args[PROC_MAX_ARGS][PROC_ARG_LEN];
        char* argv[PROC_MAX_ARGS];
        int argc = 0;
        if (copy_user_path(path, r->ebx) < 0) { r->eax = (uint32_t)-1; return; }

        if (r->ecx) {
            for (; argc < PROC_MAX_ARGS; argc++) {
                if (!user_range_ok(r->ecx + 4u * (uint32_t)argc, 4)) { r->eax = (uint32_t)-1; return; }
                uint32_t up = ((uint32_t*)r->ecx)[argc];
                if (!up) break;
                if (copy_user_str(args[argc], up, PROC_ARG_LEN) < 0) { r->eax = (uint32_t)-1; return; }
                argv[argc] = args[argc];
            }
        }
        if (argc == 0) { memcpy(args[0], path, sizeof(path)); argv[0] = args[0]; argc = 1; }

        // on success proc_exec rewrites the trap frame (eip/esp) and eax stays 0
        r->eax = (uint32_t)proc_exec(path, argv, argc, r);
        return;
    }
    case SYS_brk:
        r->eax = (uint32_t)proc_brk(r->ebx);
        return;
    case SYS_chdir: {
        char path[PROC_ARG_LEN];
        vfs_stat_t st;
        if (copy_user_path(path, r->ebx) < 0 || vfs_stat(path, &st) < 0 || !st.is_dir) { r->eax = (uint32_t)-1; return; }
        strcpy(proc_current()->cwd, path);
        r->eax = 0;
        return;
    }
    case SYS_getcwd: {
        const char* cwd = proc_current()->cwd;
        uint32_t n = strlen(cwd) + 1;
        if (n > r->ecx || !user_range_ok(r->ebx, n)) { r->eax = (uint32_t)-1; return; }
        memcpy((void*)r->ebx, cwd, n);
        r->eax = n;
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
