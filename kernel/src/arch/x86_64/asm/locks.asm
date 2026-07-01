[BITS 64]

global acquire_lock
global release_lock
global try_acquire_lock

; void acquire_lock(uintptr_t *addr, uint8_t bit)
acquire_lock:
    and  rsi, 63

.try:
    lock bts qword [rdi], rsi   ; CF = old value of bit
    jnc  .done                  ; was 0 (free) → we own it now

.spin:
    pause
    bt   qword [rdi], rsi
    jc   .spin                  ; still set → keep spinning
    jmp  .try

.done:
    ret


; int try_acquire_lock(uintptr_t *addr, uint8_t bit)
try_acquire_lock:
    and  rsi, 63
    lock bts qword [rdi], rsi
    jc   .fail
    mov  rax, 1
    ret
.fail:
    xor  rax, rax
    ret


; void release_lock(uintptr_t *addr, uint8_t bit)
release_lock:
    and  rsi, 63
    lock btr qword [rdi], rsi
    ret