%include "arch/x86_64/asm/macros.inc"
[BITS 64]

global pit_isr_entry

extern pit_timer_ticks_ms
extern _pit_timer_device_id

extern common_timer_dispatcher

pit_isr_entry:
    PUSH_ALL
    
    inc qword [rel pit_timer_ticks_ms]
    
    movsx rdi, byte [rel _pit_timer_device_id]
    mov  rsi, rsp

    call common_timer_dispatcher
    
    mov al, 0x20
    out 0x20, al

    POP_ALL
    iretq
