.section .text
.code32
.global exec_bounce_to_kernel
.type exec_bounce_to_kernel, @function

.extern g_exec_kesp
.extern g_exec_resume_eip

exec_bounce_to_kernel:
    mov g_exec_kesp, %esp      // restore kernel stack
    sti                        // <<< THIS IS THE KEY
    jmp *g_exec_resume_eip      // resume exec_enter_usermode at label