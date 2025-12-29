#include <stdint.h>
#include <arch/i386/isr.h>

extern int putchar(int);

static int sys_write(uint32_t fd, const char* buf, uint32_t len) {
    (void)fd; // for now ignore, treat as stdout
    int wrote = 0;
    for (uint32_t i = 0; i < len; i++) {
        putchar((unsigned char)buf[i]);
        wrote++;
    }
    return wrote;
}

void syscall_handle(regs_t* r) {
    uint32_t num = r->eax;

    switch (num) {
    case 4: { // write
        uint32_t fd  = r->ebx;
        const char* buf = (const char*)r->ecx;
        uint32_t len = r->edx;
        r->eax = (uint32_t)sys_write(fd, buf, len);
        return;
    }
    default:
        r->eax = (uint32_t)-1;
        return;
    }
}
