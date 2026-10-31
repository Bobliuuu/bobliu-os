#include <stdint.h>
#include <string.h>
#include <arch/i386/isr.h>
#include <arch/i386/paging.h>
#include <kernel/syscall.h>
#include <kernel/proc.h>

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

static int sys_write(uint32_t fd, const char* buf, uint32_t len) {
    if (fd != 1 && fd != 2) return -1;
    if (!user_range_ok((uint32_t)buf, len)) return -1;
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
    case SYS_execve: {
        char path[128];
        if (copy_user_str(path, r->ebx, sizeof(path)) < 0) { r->eax = (uint32_t)-1; return; }
        // on success proc_exec rewrites the trap frame (eip/esp) and eax stays 0
        r->eax = (uint32_t)proc_exec(path, r);
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
