// enter_user.S
.section .text
.code32
.global enter_user
.type enter_user, @function

.extern mark
.extern dbg_iret_eip
.extern dbg_iret_cs
.extern dbg_iret_eflags
.extern dbg_iret_esp
.extern dbg_iret_ss

// void enter_user(uint32_t entry, uint32_t user_stack_top)  (cdecl)
//
// At entry:
//   [esp+4] = entry
//   [esp+8] = user_stack_top
//
// IMPORTANT: read args BEFORE pushing anything.
enter_user:
    cli

    # --- LOAD ARGS FIRST, BEFORE ANY PUSH/POP ---
    mov 4(%esp), %edx        # entry
    mov 8(%esp), %ecx        # user_stack_top

    pusha
    pushl %edx
    call mark
    add $4, %esp
    popa

    # --- Build IRET frame: SS, ESP, EFLAGS, CS, EIP ---
    pushl $0x23              # SS (user data | RPL=3)
    pushl %ecx               # ESP (user stack top)

    pushfl
    popl  %eax
    orl   $0x200, %eax       # IF=1
    pushl %eax               # EFLAGS

    pushl $0x1B              # CS (user code | RPL=3)
    pushl %edx               # EIP (entry)

    # --- Load user data selectors ---
    mov  $0x23, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs

    iret

