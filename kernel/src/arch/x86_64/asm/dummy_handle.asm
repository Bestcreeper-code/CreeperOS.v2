[BITS 64]

global irq_dummy_handler

section .text
irq_dummy_handler:
    cli
    push rax

    ; EOI
    mov al, 0x20
    out 0xA0, al
    out 0x20, al

    pop rax
    sti
    iretq