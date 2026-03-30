bits 32
section .text

global context_switch

context_switch:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov [eax], esp

    mov esp, [esp + 24]

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret