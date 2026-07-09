#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "asm/asm.h"
#include "defines/compiler_defs.h"
#include "defines/types.h"




ssize_t syscall_handler(
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
    const __user char* filename,
    int flags,
    int mode
);

ssize_t sys_write(
    int fd,
    const __user void* buf,
    size_t count
);

ssize_t sys_read(
    int fd,
    __user void* buf,
    size_t count
);

int sys_close(
    int fd
);

#define SEEK_SET       0
#define SEEK_CUR       1
#define SEEK_END       2
loff_t sys_lseek(
    unsigned int fd,
    loff_t offset,
    unsigned int origin
);


uintptr_t sys_brk(
    size_t brk
);

int sys_mkdir(
    const __user char* path,
    int mode
);

int sys_create(
    const __user char* path,
    int mode
);

int sys_rmdir(
    const __user char* path
);

int sys_unlink(
    const __user char* path
);

void sys_exit(
    int code,
    register_t rsp
);



#endif // SYSCALLS_H