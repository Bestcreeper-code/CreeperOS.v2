#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "asm/ams.h"
#include "defines/lists.h"
#include "memory/pmm.h"

#include <stdint.h>



#define MAX_PID                     16384

#define DEFAULT_STACK_PAGE_AMOUNT   32
#define DEFAULT_STACK_PAGE_BYTES    (DEFAULT_STACK_PAGE_AMOUNT<<12)
#define STACK_UPPER_USPACE_ADDR     0x00007FFFFFFFFFFF

typedef short pid_t;

typedef struct Linked_PCB_t {
    short pid;
    uint16_t state;
#define PCB_STATE_RUNNING   0x0000
#define PCB_STATE_ZOMBIE    0x0004
    char* name;

    Stack_t kernel_stack, user_stack;

    uint64_t k_rsp;

    int exit_code;

    uintptr_t cr3;

    struct hlist_node list_node;
} Linked_PCB_t;

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} ProcessStackFrame;



extern Linked_PCB_t* _scheduler_current_process;
extern struct hlist_head _scheduler_process_list_head;
extern uint8_t task_switching_flag;

int scheduler_init();

void* sched_next_process_core(uint64_t saved_rsp);

Linked_PCB_t* new_pcb(physptr_t page_dir, const char* name, uint64_t* rsp, Stack_t k_stack, Stack_t us_stack);


int kill_ktask(Linked_PCB_t* pcb);


void _build_kernel_stack_frame(uint64_t* stack_top, uint64_t entry);
void _build_user_stack_frame(uint64_t** stack_top, uint64_t entry,
    uint64_t user_rsp, uint16_t cs, uint16_t ss);

Linked_PCB_t* ktask_start(void* entry, char* name);
Linked_PCB_t* us_task_start(void* entry, char* name, uintptr_t page_dir);
void enable_scheduler();
void disable_scheduler();

void yield_core(uintptr_t sp);
void _yield();


void _sched_next_process();

#endif // SCHEDULER_H