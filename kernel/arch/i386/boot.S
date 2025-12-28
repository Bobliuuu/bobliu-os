# Declare constants for the multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Extern
.extern kernel_early
.extern call_global_constructors
.extern kernel_main

# Declare a header as in the Multiboot Standard.
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Reserve a stack for the initial thread.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# The kernel entry point.
.section .text
.global _start
.type _start, @function

# start method
_start:
    movl $stack_top, %esp

    # Save multiboot regs immediately (GRUB gives them in eax/ebx)
    push %eax            # magic
    push %ebx            # mbi

    # ---- early init (magic, mbi) ----
    movl 4(%esp), %eax   # reload saved magic
    movl 0(%esp), %ebx   # reload saved mbi
    push %ebx
    push %eax
    call kernel_early
    add $8, %esp

    # ---- constructors ----
    call call_global_constructors
	
	# VGA marker: write 'A' at cell 4
    mov $0xB8000, %edi
    movw $0x1F41, 8(%edi)

    # ---- kernel_main (magic, mbi) ----
    movl 4(%esp), %eax   # reload saved magic
    movl 0(%esp), %ebx   # reload saved mbi
    push %ebx
    push %eax
    call kernel_main
    add $8, %esp

    cli
1:  hlt
    jmp 1b
.size _start, . - _start
