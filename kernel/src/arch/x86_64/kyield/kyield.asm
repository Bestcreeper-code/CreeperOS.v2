[BITS 64]
extern sched_next_process_core
extern _ret_to_next_process
global _kernel_yield

%define KERNEL_CS  0x08
%define KERNEL_SS  0x10

section .text
_kernel_yield:
    pop rax                  ; return rip
    mov rcx, rsp               ; resume-time rsp

    push KERNEL_SS              ; ss
    push rcx                    ; rsp
    pushfq                       ; rflags — captured while IF is still genuinely enabled
    cli                           ; NOW block interrupts for the rest of the build + switch
    push KERNEL_CS              ; cs
    push rax                    ; rip

    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call sched_next_process_core
    jmp _ret_to_next_process