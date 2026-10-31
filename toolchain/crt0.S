/* user program entry: _start -> main(argc, argv), then exit(main's return value)
   kernel leaves [argc][argv0][argv1]...[NULL] at the initial esp */
.section .text
.global _start
_start:
    xorl %ebp, %ebp
    movl (%esp), %eax        /* argc */
    leal 4(%esp), %edx       /* argv */
    pushl %edx
    pushl %eax
    call main
    movl %eax, %ebx
    movl $1, %eax            /* SYS_exit */
    int $0x80
1:  jmp 1b
