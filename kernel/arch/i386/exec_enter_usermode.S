.section .text
.code32
.global exec_enter_usermode
.type exec_enter_usermode, @function


.extern g_exec_kesp
.extern g_exec_resume_eip
.extern g_exec_kcr3          # NEW
.extern enter_user
.extern exec_debug
.extern mark

exec_enter_usermode:
    mov %esp, g_exec_kesp

    mov $exec_resume_after_user, %eax
    mov %eax, g_exec_resume_eip

    mov 4(%esp), %edx      # entry
    mov 8(%esp), %ecx      # user_stack_top

    pushl %ecx
    pushl %edx
    call enter_user

    ud2

exec_resume_after_user:
    jmp exec_return_to_shell
