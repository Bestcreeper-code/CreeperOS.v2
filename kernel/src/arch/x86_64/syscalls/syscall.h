#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "asm/asm.h"
#include "defines/types.h"


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

int sys_open(
    register_t filename,
    register_t flags,
    register_t mode
);

int sys_write(
    register_t fd,
    register_t buf,
    register_t count
);

ssize_t sys_read(
    register_t fd,
    register_t buf,
    register_t count
);

int sys_close(register_t fd);



int sys_mkdir(register_t path, register_t mode);

int sys_create(register_t path, register_t mode);

int sys_rmdir(register_t path);

int sys_unlink(register_t path);

void sys_exit(
    register_t code,
    register_t rsp
);


#endif // SYSCALLS_H