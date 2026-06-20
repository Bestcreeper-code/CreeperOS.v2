[BITS 64]
global _sched_next_process
global _ret_to_next_process

extern sched_next_process_core

section .text
_sched_next_process:
    cli
    push rsp
    mov rdi, rsp
    call sched_next_process_core
_ret_to_next_process:
    mov rsp, rax
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    iretq