/* user program entry: _start -> main(argc, argv), then exit(main's return value)
   kernel leaves [argc][argv0][argv1]...[NULL] at the initial esp */
.section .text
.global _start
_start:
    xorl %ebp, %ebp
    fninit                   /* x87: 53-bit precision so double math rounds like double (musl's exp/log rely on it) */
    subl $4, %esp
    movw $0x027F, (%esp)
    fldcw (%esp)
    addl $4, %esp
    movl (%esp), %eax        /* argc */
    leal 4(%esp), %edx       /* argv */
    leal 8(%esp,%eax,4), %ecx /* envp = argv + argc + 1 */
    movl %ecx, environ
    pushl %edx
    pushl %eax
    call main
    pushl %eax
    call exit                /* flushes stdio, runs atexit handlers, then SYS_exit */
    movl %eax, %ebx
    movl $1, %eax            /* SYS_exit */
    int $0x80
1:  jmp 1b
