bits 32
section .text
global tss_flush

tss_flush:
    mov ax, 0x18
    ltr ax
    ret