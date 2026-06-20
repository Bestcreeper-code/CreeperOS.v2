[BITS 64]
section .text

extern irq_dummy_common

%macro ISR_STUB 1
global irq_stub_%1
irq_stub_%1:
    mov  rdi, %1
    jmp  irq_dummy_common_wrap
%endmacro


irq_dummy_common_wrap:
    ; save registers
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; align stack to 16 bytes 
    sub  rsp, 8

    call irq_dummy_common

    add  rsp, 8

    ; restore registers 
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    
    iretq

%assign i 0
%rep 256
    ISR_STUB i
    %assign i i+1
%endrep


section .data
global irq_stub_table
irq_stub_table:
%assign i 0
%rep 256
    dq irq_stub_%+i
    %assign i i+1
%endrep