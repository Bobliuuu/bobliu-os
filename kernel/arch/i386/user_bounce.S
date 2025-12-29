.global usermode_return_to_kernel
.extern g_exec_kesp
.extern g_exec_resume_eip

// Switch back to kernel stack and jump to resume label.
// Called ONLY from ring0 (inside syscall handler).
usermode_return_to_kernel:
    mov g_exec_kesp, %esp
    jmp *g_exec_resume_eip
