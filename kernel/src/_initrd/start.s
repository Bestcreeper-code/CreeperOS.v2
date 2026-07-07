global _start
extern main

section .text

_start:
    call main

    mov rax,60
    mov rdi,0
    syscall

.hang:
    jmp .hang