[BITS 64]

extern syscall_handler
extern _ret_to_next_process

global _syscall_entry


%define USER_CS 0x23
%define USER_SS 0x1B

section .bss
align 8

_syscall_user_rsp_scratch: resq 1

section .text
_syscall_entry:
    
    mov [rel _syscall_user_rsp_scratch], rsp

    ; iretq frame 
    push USER_SS                                ; ss
    push qword [rel _syscall_user_rsp_scratch]   ; user rsp
    push r11                                     ; rflags
    push USER_CS                                 ; cs
    push rcx                                     ; rip

    ; sched frame
    push rax            ; syscall number
    push rcx

    push rdx            ; arg3
    push rbx
    push rbp
    push rsi             ; arg2
    push rdi             ; arg1
    push r8              ; arg5
    push r9              ; arg6
    push r10             ; arg4
    push r11             

    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi       ; arg1
    mov rbp, rsi       ; arg2
    mov r12, rdx       ; arg3
    mov r13, r10       ; arg4
    mov r14, r8        ; arg5
    mov r15, r9        ; arg6

    mov rdi, rax        ; syscall number
    mov rsi, rbx        ; arg1
    mov rdx, rbp        ; arg2
    mov rcx, r12        ; arg3
    mov r8,  r13        ; arg4
    mov r9,  r14        ; arg5

    mov rax, rsp        ; stash rsp

    sub rsp, 8              
    push r15                ; arg6
    push rax                ; rsp

    call syscall_handler

    add rsp, 24

    mov rax, rsp
    jmp _ret_to_next_process