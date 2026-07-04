%include "arch/x86_64/asm/macros.inc"
[BITS 64]

global pit_isr_entry

extern pit_timer_ticks_ms
extern _pit_timer_device_id
extern _pit_timer_irq_vector

extern _current_eoi

extern common_timer_dispatcher

pit_isr_entry:
    PUSH_ALL
    
    inc qword [rel pit_timer_ticks_ms]
    
    movsx rdi, byte [rel _pit_timer_device_id]
    mov  rsi, rsp

    call common_timer_dispatcher

    mov dil, byte [rel _pit_timer_irq_vector]
    mov rax, [_current_eoi]
    call rax   

    POP_ALL
    iretq
