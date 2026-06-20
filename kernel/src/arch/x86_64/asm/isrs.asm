[BITS 64]
extern _panic_handler
%define MAX_STACK_TRACE_SIZE 16

%macro SAVE_ALL 0
    mov  rax, cr8
    push rax
    mov  rax, cr4
    push rax
    mov  rax, cr3
    push rax
    mov  rax, cr2
    push rax
    mov  rax, cr0
    push rax

    sub  rsp, 2
    mov  word [rsp], ss
    sub  rsp, 2
    mov  word [rsp], gs
    sub  rsp, 2
    mov  word [rsp], fs
    sub  rsp, 2
    mov  word [rsp], es
    sub  rsp, 2
    mov  word [rsp], ds
    sub  rsp, 2
    mov  word [rsp], cs

    pushfq

    lea  rax, [rel $] ;<<<<<
    push rax

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8

    mov  rax, [rsp + 172]
    push rax

    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    xor  rax, rax
    push rax
%endmacro

%macro RESTORE_ALL 0
    pop  rax
    pop  rbx
    pop  rcx
    pop  rdx
    pop  rsi
    pop  rdi
    pop  rbp
    add  rsp, 8
    pop  r8
    pop  r9
    pop  r10
    pop  r11
    pop  r12
    pop  r13
    pop  r14
    pop  r15
    add  rsp, 68
%endmacro

section .bss
align 8
isr_call_stack  resq MAX_STACK_TRACE_SIZE

section .text
isr_common_handler:
    mov  rax, [rsp + 128]
    mov  [isr_call_stack], rax

    mov  rbp, [rsp + 48]
    mov  rcx, 1
.trace_loop:
    cmp  rcx, MAX_STACK_TRACE_SIZE
    jge  .trace_done
    test rbp, rbp
    jz   .trace_done
    test rbp, 7
    jnz  .trace_done
    mov  rax, [rbp + 8]
    test rax, rax
    jz   .trace_done
    mov  [isr_call_stack + rcx*8], rax
    inc  rcx
    mov  rbp, [rbp]
    jmp  .trace_loop
.trace_done:
    cmp  rcx, MAX_STACK_TRACE_SIZE
    jge  .zero_done
.zero_loop:
    mov  qword [isr_call_stack + rcx*8], 0
    inc  rcx
    cmp  rcx, MAX_STACK_TRACE_SIZE
    jl   .zero_loop
.zero_done:
    mov  rdi, [rsp + 196]
    mov  rsi, [rsp + 204]
    lea  rdx, [rsp]
    lea  rcx, [isr_call_stack]
    sub  rsp, 8
    call _panic_handler
    add  rsp, 8

    RESTORE_ALL
    add  rsp, 16
    iretq

%macro ISR_NOCRASH 1
global isr%1
isr%1:
    iretq
%endmacro

%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push qword 0
    push qword %1
    SAVE_ALL
    jmp  isr_common_handler
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push qword %1
    push qword 0
    SAVE_ALL
    jmp  isr_common_handler
%endmacro

ISR_NOCRASH  4
ISR_NOCRASH  7
ISR_NOCRASH  9
ISR_NOCRASH 15
ISR_NOCRASH 16
ISR_NOERR  0
ISR_NOERR  1
ISR_NOERR  2
ISR_NOERR  3
ISR_NOERR  5
ISR_NOERR  6
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31
ISR_ERR  8
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_ERR 17