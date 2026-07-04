BITS 64

section .text
global _start

_start:
    ; write(1, msg, msg_len)
    mov rax, 1          ; SYS_write
    mov rdi, 1          ; fd = stdout
    mov rsi, msg        ; buffer
    mov rdx, msg_len    ; length
    syscall

    ; yield()
    mov rax, 2          ; SYS_yield
    syscall

    ; write again
    mov rax, 1
    mov rdi, 1
    mov rsi, msg2
    mov rdx, msg2_len
    syscall

    ; exit(0)
    mov rax, 3          ; SYS_exit
    xor rdi, rdi        ; status = 0
    syscall

.hang:
    jmp .hang

section .rodata
msg db "user mode active\n"
msg_len equ $ - msg

msg2 db "second syscall works\n"
msg2_len equ $ - msg2