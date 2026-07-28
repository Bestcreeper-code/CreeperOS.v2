#pragma once

#include "asm/asm.h"
#include "defines/compiler_defs.h"
#include "defines/types.h"
#include "scheduler/scheduler.h"
#include "timer/timers.h"
#include "timer/time.h"


#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3


#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MPROTECT 10
#define SYS_MUNMAP 11
#define SYS_BRK 12

#define SYS_ACCESS 21

#define SYS_SCHED_YIELD 24

#define SYS_DUP 32
#define SYS_DUP2 33

#define SYS_GETPID 39

#define SYS_FORK 57

#define SYS_EXECVE 59
#define SYS_EXIT 60

#define SYS_UNAME 63

#define SYS_GETCWD 79
#define SYS_CHDIR 80

#define SYS_MKDIR 83
#define SYS_RMDIR 84
#define SYS_CREATE 85

#define SYS_UNLINK 87

#define SYS_ARCH_PRCTL 158

#define SYS_GETDENTS64 217
#define SYS_CLOCK_GETTIME 228

#define SYS_OPENAT 257
#define SYS_MKDIRAT 258

#define SYS_UNLINKAT 263

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

#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04
int sys_mprotect(
    uintptr_t start,
    size_t len,
    unsigned long prot
);

int sys_munmap(
    uintptr_t addr,
    size_t len
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

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};
int sys_uname(
    __user struct utsname* ptr 
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

int sys_clock_gettime(
    const clockid_t which_clock,
    __user timespec* tim_sp
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

