// gdt_flush.s
.global gdt_flush
gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)

    mov $0x10, %ax      // data selector = 2nd descriptor * 8 = 0x10
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    ljmp $0x08, $flush2 // code selector = 1st descriptor * 8 = 0x08
flush2:
    ret
