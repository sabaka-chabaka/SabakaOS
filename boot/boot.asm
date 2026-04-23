bits 32

MAGIC    equ 0x1BADB002
FLAGS    equ (1<<0) | (1<<1) | (1<<2)
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

    dd 0
    dd 0
    dd 0
    dd 0
    dd 0

    dd 0
    dd 1024
    dd 768
    dd 32

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang