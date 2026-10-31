#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

/*
 * syscall ABI (int 0x80), shared by kernel and user programs.
 *   eax = number, ebx/ecx/edx = args 1-3, return value in eax (-1 on error)
 * numbers follow linux i386 so existing docs/toolchains line up.
 */
#define SYS_exit    1
#define SYS_write   4
#define SYS_getpid  20

#ifndef __ASSEMBLER__
static inline int __syscall3(int num, int a, int b, int c) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return ret;
}
#endif

#endif
