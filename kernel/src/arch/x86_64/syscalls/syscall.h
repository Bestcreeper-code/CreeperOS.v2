#pragma once

#include "asm/asm.h"
#include "defines/compiler_defs.h"
#include "defines/types.h"
#include "scheduler/scheduler.h"



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

pid_t sys_getpid();

pid_t sys_fork(register_t rsp);



#define EXEC_ARGV_MAX   64
#define EXEC_ARG_MAXLEN 256
int sys_execve(
    const __user char* filename,
    const __user char* argv[],
    const __user char* envp[],
    register_t rsp
);

void sys_exit(
    int code,
    register_t rsp
);

char* sys_getcwd(
    __user char* buf,
    unsigned long size
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


int sys_access(
    const __user char* path,
    int mode
);

int sys_dup2(
    int oldfd,
    int newfd
);

int sys_dup(
    int oldfd
);

int sys_chdir(
    const __user char* path
);

#define ARCH_PRCTL_SET_FS 0x1002
long sys_arch_prctl(
    int code,
    unsigned long* addr
);

ssize_t sys_getdents64(
    unsigned int fd,
    __user void* dirp,
    size_t count
);

int sys_openat(
    int dfd,
    const __user char* filename,
    int flags,
    int mode
);

int sys_mkdirat(
    int dfd,
    const __user char* path,
    int mode
);

int sys_unlinkat(
    int dfd,
    const __user char* pathname,
    int flag
);

