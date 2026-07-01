#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "asm/asm.h"

#include <stdint.h>

int syscall_handler(
    register_t rax, //syscall number
    register_t rdi, //arg1
    register_t rsi, //arg2
    register_t rdx, //arg3
    register_t r10, //arg4
    register_t r8,  //arg5
    register_t r9,  //arg6
    register_t rsp  //stack ptr
);

int sys_write(
    register_t fd,
    register_t buf,
    register_t count
);

void sys_exit(
    register_t code,
    register_t rsp
);

#endif // SYSCALLS_H