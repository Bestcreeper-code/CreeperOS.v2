extern main

section .text
global _start

_start:
    xor ebp, ebp             ; mark outermost stack frame (ABI convention, helps backtraces)
    mov rbx, rsp

    ; argc
    mov rdi, [rbx]

    ; argv = rsp + 8
    lea rsi, [rbx + 8]

    ; find envp:
    ; skip argv pointers until NULL
    mov rcx, rsi
.find_argv_end:
    cmp qword [rcx], 0
    je .argv_done
    add rcx, 8
    jmp .find_argv_end

.argv_done:
    add rcx, 8              ; rcx = envp

    ; envp = rcx
    mov rdx, rcx

    ; find end of envp
.find_env_end:
    cmp qword [rcx], 0
    je .env_done
    add rcx, 8
    jmp .find_env_end

.env_done:
    add rcx, 8              ; skip envp NULL
    ; rcx now points at auxv, which we intentionally skip

    and rsp, -16            ; defensive re-align (rsp is already 16B-aligned here per ABI,
                             ; this just guards against any future edits adding pushes above)
    call main

    mov edi, eax             ; zero-extend main's 32-bit int return into rdi
    mov rax, 60               ; sys_exit
    syscall

.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite align=1