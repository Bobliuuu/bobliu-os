# int setjmp(jmp_buf): saves ebx esi edi ebp esp eip; returns 0
# void longjmp(jmp_buf, int val): returns val (1 if 0) from the matching setjmp
.text
.global setjmp
.global _setjmp
.type setjmp, @function
setjmp:
_setjmp:
    movl 4(%esp), %eax
    movl %ebx, 0(%eax)
    movl %esi, 4(%eax)
    movl %edi, 8(%eax)
    movl %ebp, 12(%eax)
    leal 4(%esp), %ecx        # esp as it will be after return
    movl %ecx, 16(%eax)
    movl 0(%esp), %ecx        # return address
    movl %ecx, 20(%eax)
    xorl %eax, %eax
    ret

.global longjmp
.global _longjmp
.type longjmp, @function
longjmp:
_longjmp:
    movl 4(%esp), %edx
    movl 8(%esp), %eax
    testl %eax, %eax
    jnz 1f
    incl %eax
1:
    movl 0(%edx), %ebx
    movl 4(%edx), %esi
    movl 8(%edx), %edi
    movl 12(%edx), %ebp
    movl 16(%edx), %esp
    jmp *20(%edx)
