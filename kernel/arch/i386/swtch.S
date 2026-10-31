.section .text
.code32

/* void swtch(struct context** old, struct context* new) */
.global swtch
swtch:
    movl 4(%esp), %eax
    movl 8(%esp), %edx
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, (%eax)
    movl %edx, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

/* first time a new process is switched to, swtch's ret lands here with esp
   pointing at its trap frame (regs_t): unwind it like isr_common does. */
.global trapret
trapret:
    popl %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    popa
    addl $8, %esp
    iret
