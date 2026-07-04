%include "arch/x86_64/asm/macros.inc"

[BITS 64]

global apic_timer_interrupt_handler

extern c_apic_timer_interrupt_handler



apic_timer_interrupt_handler:
    PUSH_ALL

    mov rdi, rsp
    call c_apic_timer_interrupt_handler

    POP_ALL
    iretq