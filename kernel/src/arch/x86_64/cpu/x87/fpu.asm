

global x87_fpu_try_config

extern  

%define CR0_MP  (1 << 1)
%define CR0_EM  (1 << 2)
%define CR0_TS  (1 << 3)

%define CR4_OSFXSR     (1 << 9)
%define CR4_OSXMMEXCPT (1 << 10)

section .text

x87_fpu_try_config:

    ; Enable x87/SSE usage
    mov rax, cr0

    ; Set MP, clear EM and TS
    or  rax, CR0_MP
    and rax, ~(CR0_EM | CR0_TS)

    mov cr0, rax


    ; Enable SSE state management
    mov rax, cr4

    or rax, CR4_OSFXSR
    or rax, CR4_OSXMMEXCPT

    mov cr4, rax


    ; Initialize x87
    fninit

    ; Check x87 actually responds
    mov word [testword], 0x1111
    fnstsw [testword]

    cmp word [testword], 0
    jne .nofpu


    ; Initialize SSE control register
    mov eax, 0x1F80        ; default MXCSR
    ldmxcsr [mxcsr_default]

    mov eax, 1
    ret


.nofpu:
    xor eax, eax
    ret


section .data
align 4

testword:
    dw 0

align 16
mxcsr_default:
    dd 0x1F80