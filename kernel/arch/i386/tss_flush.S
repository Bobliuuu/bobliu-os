.global tss_flush
tss_flush:
    mov $0x28, %ax       # selector for GDT entry 5
    ltr %ax
    ret
