// arch/i386/cpu/enter_user.S
.global enter_user
.type enter_user, @function

// void enter_user(uint32_t entry, uint32_t user_stack_top);
// cdecl args:
//   [esp+4] = entry
//   [esp+8] = user_stack_top

enter_user:
    cli

    // Save args BEFORE touching the stack
    mov  4(%esp), %ecx        // ecx = entry (EIP)
    mov  8(%esp), %edx        // edx = user_stack_top (ESP)

    // Load USER_DS into segment registers (RPL=3 selector is fine here)
    mov  $0x23, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs

    // Build iret frame: SS, ESP, EFLAGS, CS, EIP
    pushl $0x23               // SS = USER_DS
    pushl %edx                // ESP = user_stack_top

    pushfl
    popl %eax
    orl  $0x200, %eax         // IF=1
    pushl %eax                // EFLAGS

    pushl $0x1B               // CS = USER_CS
    pushl %ecx                // EIP = entry

    iret
