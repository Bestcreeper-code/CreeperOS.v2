global x87_fpu_try_config

%define CR0_EM 0x4
%define CR0_TS 0x8

section .text

x87_fpu_try_config:

;     pop edx
;     mov edx, cr0                            ; Start probe, get CR0
;     and edx, (-1) - (CR0_TS + CR0_EM)       ; clear TS and EM to force fpu access
;     mov cr0, edx                            ; store control word
;     fninit                                  ; load defaults to FPU
;     fnstsw [.testword]                      ; store status word
;     cmp word [.testword], 0                 ; compare the written status with the expected FPU state
;     jne .nofpu                              ; jump if the FPU hasn't written anything (i.e. it's not there)
;     jmp .hasfpu

; .nofpu:
;     mov eax, 0
;     ret
; .hasfpu:
;     mov eax, 1
    ret

.testword: dw 0x1111 