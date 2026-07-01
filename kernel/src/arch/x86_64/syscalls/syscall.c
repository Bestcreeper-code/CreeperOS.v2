#include "syscall.h"
#include "asm/asm.h"

#include "cpu/gdt.h"
#include "debug/Logger.h"
#include "drivers/drivers.h"
#include "scheduler/scheduler.h"

#define MSR_STAR            0xC0000081
#define MSR_LSTAR           0xC0000082
#define MSR_SYSCALL_MASK    0xC0000084
#define MSR_EFER            0xC0000080

extern void _syscall_entry();

int syscall_init() {

    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);

    uint64_t star = ((uint64_t)(USER_CODE_SEGMENT - 16) << 48) |
                    ((uint64_t)(KERNEL_CODE_SEGMENT)     << 32);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)_syscall_entry);
    wrmsr(MSR_SYSCALL_MASK, (1 << 9) | (1 << 10)); // IF | DF
}
REGISTER_DRIVER_CORE(syscalls, syscall_init);

int syscall_handler(
    register_t rax, //syscall number
    register_t rdi, //arg1
    register_t rsi, //arg2
    register_t rdx, //arg3
    register_t r10, //arg4
    register_t r8,  //arg5
    register_t r9,  //arg6
    register_t rsp  //stack ptr
) {
#if (SYSCALL_DEBUG)
    Sys_log("syscall: rax=%lx, rdi=%lx, rsi=%lx, rdx=%lx, r10=%lx, r8=%lx, r9=%lx \n",
        rax, rdi, rsi, rdx, r10, r8, r9);
#endif

    switch (rax) {
        case 1: // sys_exit
            sys_exit(rdi, rsp);
            return 0;

        case 4: // sys_write
            return sys_write(rdi, rsi, rdx); // fd, buf addr, count

        case 158: // sys_yield
            yield_core(rsp);
            return 0;

        default:
            Sys_log("Unknown syscall: %lx\n", rax);
            return -1;
    }
}

int sys_write(
    register_t fd,
    register_t buf,
    register_t count
) {
    Sys_log("write called: fd=%d, buf=%p, count=%ld\n",
        (int)fd, (void*)buf, count);
    return 0;
}

void sys_exit(register_t code, register_t rsp) {
    _scheduler_current_process->exit_code = code;
    _scheduler_current_process->state     = PCB_STATE_ZOMBIE;

    yield_core(rsp);
}