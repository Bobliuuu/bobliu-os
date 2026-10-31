#include <stdint.h>
#include <arch/i386/isr.h>
#include <arch/i386/user_bouncing.h>
#include <kernel/syscall.h>

extern int putchar(int);

#define USER_LO 0x00400000u
#define USER_HI 0x02000000u

// user buffer must sit entirely inside the user region
static int user_range_ok(uint32_t p, uint32_t len) {
    if (p < USER_LO || p >= USER_HI) return 0;
    if (len > USER_HI - p) return 0;
    return 1;
}

static int sys_write(uint32_t fd, const char* buf, uint32_t len) {
    if (fd != 1 && fd != 2) return -1;
    if (!user_range_ok((uint32_t)buf, len)) return -1;
    for (uint32_t i = 0; i < len; i++) putchar((unsigned char)buf[i]);
    return (int)len;
}

void syscall_handle(regs_t* r) {
    switch (r->eax) {
    case SYS_exit:
        g_user_exited = 1;   // isr stub bounces back to the kernel after unwinding
        return;
    case SYS_write:
        r->eax = (uint32_t)sys_write(r->ebx, (const char*)r->ecx, r->edx);
        return;
    case SYS_getpid:
        r->eax = 1;          // single process for now
        return;
    default:
        r->eax = (uint32_t)-1;
        return;
    }
}
