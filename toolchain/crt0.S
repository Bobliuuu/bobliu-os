/* user program entry: _start -> main(), then exit(main's return value) */
.section .text
.global _start
_start:
    xorl %ebp, %ebp
    call main
    pushl %eax          /* unused by int 0x80, kept for debuggers */
    movl %eax, %ebx
    movl $1, %eax       /* SYS_exit */
    int $0x80
1:  jmp 1b
