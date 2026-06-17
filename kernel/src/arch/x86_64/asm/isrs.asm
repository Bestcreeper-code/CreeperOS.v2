[BITS 64]

extern _panic_handler

%define MAX_STACK_TRACE_SIZE 3

%macro SAVE_ALL 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    
    pushfq

    lea rax, [rel $]
    push rax

    xor rax, rax
    mov ax, cs
    push rax
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax
    mov ax, ss
    push rax
    
    mov rax, cr0
    push rax
    mov rax, cr2
    push rax
    mov rax, cr3
    push rax
    mov rax, cr4
    push rax
    mov rax, cr8
    push rax
%endmacro


%macro RESTORE_ALL 0
    add rsp, 13*8
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
%endmacro


section .bss
align 8
isr_call_stack  resq MAX_STACK_TRACE_SIZE


section .text

isr_common_handler:
    
    mov  rax, [rsp + 11*8]
    mov  [isr_call_stack], rax

    
    mov  rbp, [rsp + 19*8]
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

    ; _panic_handler(index, err_code, regs*, call_stack*) 
    mov  rdi, [rsp + 28*8]
    mov  rsi, [rsp + 29*8]
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
    SAVE_ALL
    jmp  isr_common_handler
%endmacro

ISR_NOCRASH  4    ; overflow (#OF)
ISR_NOCRASH  7    ; device not available (#NM) – handle in C if needed
ISR_NOCRASH  9    ; coprocessor segment overrun (obsolete, never fires)
ISR_NOCRASH 15    ; reserved
ISR_NOCRASH 16    ; x87 floating-point (#MF)

ISR_NOERR  0      ; divide-by-zero (#DE)
ISR_NOERR  1      ; debug (#DB)
ISR_NOERR  2      ; non-maskable interrupt (NMI)
ISR_NOERR  3      ; breakpoint (#BP)
ISR_NOERR  5      ; bound range exceeded (#BR)
ISR_NOERR  6      ; invalid opcode (#UD)
ISR_NOERR 18      ; machine check (#MC)
ISR_NOERR 19      ; SIMD floating-point (#XM / #XF)
ISR_NOERR 20      ; virtualisation (#VE)
ISR_NOERR 21      ; control-protection (#CP)
ISR_NOERR 22      ; reserved
ISR_NOERR 23      ; reserved
ISR_NOERR 24      ; reserved
ISR_NOERR 25      ; reserved
ISR_NOERR 26      ; reserved
ISR_NOERR 27      ; reserved
ISR_NOERR 28      ; hypervisor injection (#HV)
ISR_NOERR 29      ; VMM communication (#VC)
ISR_NOERR 30      ; security exception (#SX)
ISR_NOERR 31      ; reserved

ISR_ERR  8        ; double fault (#DF)       – error code always 0
ISR_ERR 10        ; invalid TSS (#TS)
ISR_ERR 11        ; segment not present (#NP)
ISR_ERR 12        ; stack-segment fault (#SS)
ISR_ERR 13        ; general protection (#GP)
ISR_ERR 14        ; page fault (#PF)
ISR_ERR 17        ; alignment check (#AC)