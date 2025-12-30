.global usermode_return_to_kernel
.extern g_exec_kesp
.extern g_exec_resume_eip
.extern g_exec_kcr3

usermode_return_to_kernel:
usermode_return_to_kernel:
    mov g_exec_kcr3, %eax
    mov %eax, %cr3

    mov g_exec_kesp, %esp

    sti                 # <<< ADD THIS
    jmp *g_exec_resume_eip
