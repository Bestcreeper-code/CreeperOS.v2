%include "arch/x86_64/asm/macros.inc"
[BITS 64]

extern syscall_handler
extern _ret_to_next_process
extern _tss

global _syscall_entry

%define USER_CS 0x23
%define USER_SS 0x1B
%define TSS_RSP0_OFFSET 4

section .bss
align 8

_syscall_user_rsp_scratch: resq 1

section .text
_syscall_entry: ;enters with interrupts disabled
    mov [rel _syscall_user_rsp_scratch], rsp ; stash user rsp
    mov rsp, [rel _tss + TSS_RSP0_OFFSET] ; this process's kernel stack

    ; iretq frame  (bullshit way to make yield work.. might change it later if i remember to)
    push USER_SS ; ss
    push qword [rel _syscall_user_rsp_scratch] ; user rsp
    push r11 ; rflags
    push USER_CS ; cs
    push rcx ; rip

    PUSH_ALL
sti
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
    push rax                ; rsp(arg7)
    push r15                ; arg6

    call syscall_handler

    add rsp, 24              ;clean args

    ;change the saved rax to the ret addr
    mov [rsp + 14*8], rax

    POP_ALL

    ; stack now holds the iretq frame
    pop rcx                  ; rip
    add rsp, 8               ; skip cs
    pop r11                  ; rflags
    pop rsp                  ; user rsp

    o64 sysret

