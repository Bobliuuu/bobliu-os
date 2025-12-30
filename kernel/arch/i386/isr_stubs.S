// isr_stubs.s
.extern isr_handler
.extern irq_handler
.extern g_user_exited
.extern exec_bounce_to_kernel
.extern g_exec_kesp
.extern g_exec_resume_eip

// Pushes a dummy error code for exceptions that don't have one.
.macro ISR_NOERR n
.global isr\n
isr\n:
    cli
    pushl $0          // err_code
    pushl $\n         // int_no
    jmp isr_common
.endm

// For exceptions that DO push an error code, CPU already pushed it.
.macro ISR_ERR n
.global isr\n
isr\n:
    cli
    pushl $\n
    jmp isr_common
.endm

// IRQ macro
.macro IRQ n vec
.global irq\n
irq\n:
    cli
    pushl $0          // err_code
    pushl $\vec       // int_no (32+irq)
    jmp irq_common
.endm

isr_common:
    pusha
    mov %ds, %ax
    pushl %eax

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    pushl %esp
    call isr_handler
    add $4, %esp

    popl %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    popa
    add $8, %esp      // int_no + err_code

    // NEW: if user requested exit, bounce now (AFTER unwinding)
    cmpl $0, g_user_exited
    je  1f

    movl $0, g_user_exited

    // if you saved kernel cr3, restore it here
    // mov g_exec_kcr3, %eax
    // mov %eax, %cr3

    mov g_exec_kesp, %esp

    sti                    // CRITICAL: re-enable IRQs (keyboard)
    jmp *g_exec_resume_eip  // resumes at exec_resume_after_user

1:
    sti
    iret

irq_common:
    pusha
    mov %ds, %ax
    pushl %eax

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    pushl %esp
    call irq_handler
    add $4, %esp

    popl %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    popa
    add $8, %esp
    sti
    iret

// Exceptions 0..31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

// IRQs 0..15 mapped to vectors 32..47
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47
ISR_NOERR 128