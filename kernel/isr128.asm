bits 32
section .text

extern irq_handler

global isr128

isr128:
    ; NOTE: do NOT cli here — int 0x80 is a software interrupt.
    ; Keeping interrupts enabled lets the PIT fire normally during syscalls.
    ; The iret at the end restores the caller's original eflags (incl. IF).
    push dword 0
    push dword 0x80
    jmp isr128_common

isr128_common:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call irq_handler
    pop eax
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    iret