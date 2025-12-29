// arch/i386/cpu/exec_bounce.S
.global exec_bounce_to_kernel
.type exec_bounce_to_kernel, @function

.extern g_exec_kesp
.extern g_exec_resume_eip

exec_bounce_to_kernel:
    mov g_exec_kesp, %esp
    jmp *g_exec_resume_eip
