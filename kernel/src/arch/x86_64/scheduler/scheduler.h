#pragma once
#include "arch/vmm.h"
#include "asm/asm.h"
#include "defines/compiler_defs.h"
#include "defines/lists.h"
#include "memory/pmm.h"
#include "vfs/vfs.h"


#include <stdint.h>



#define MAX_PID                     16384

#define DEFAULT_STACK_PAGE_AMOUNT   128
#define DEFAULT_STACK_PAGE_BYTES    (DEFAULT_STACK_PAGE_AMOUNT<<12)
#define STACK_UPPER_USPACE_ADDR     0x0000800000000000ULL

#define DEFAULT_FILE_TABLE_PAGES    4 //256 entries if file struct =64 bytes
#define DEFAULT_FILE_TABLE_START    0x00007E0000000000 //2TiB under stack start

#define DEFAULT_FILE_TABLE_ENTRIES  DEFAULT_FILE_TABLE_PAGES * (PAGE_SIZE_4K/ sizeof(file))


#define DEFAULT_USER_HEAP_PAGES     16 //64KB
#define DEFAULT_USER_HEAP_START     0x0000600000000000 //32TiB under stack start/upper
#define DEFAULT_USER_HEAP_END       (0x0000600000000000 + \
                DEFAULT_USER_HEAP_PAGES * PAGE_SIZE_4K)

                
#define USER_ARGS_MAX_PAGES         16
#define USER_ARGS_USPACE_ADDR       ((DEFAULT_USER_HEAP_START - PAGE_SIZE_4K*(USER_ARGS_MAX_PAGES+1)))



typedef short pid_t;
typedef short tid_t;

typedef struct linked_pcb_t {
    short pid;
    uint16_t state;
#define PCB_STATE_RUNNING       0x0001
#define PCB_STATE_STARTING      0x0002
#define PCB_STATE_DEAD        0x0004
    char* name;

    kuid_t uid;
    kgid_t gid;

    heap_t heap;
    stack_t kernel_stack, user_stack;

    uint64_t k_rsp;

    int exit_code;

    physptr_t cr3;

    uintptr_t opened_file_table;

    struct inode* cwd_i;

    uintptr_t fs_base, gs_base;

    GCC_ATTR((aligned(16))) uint8_t fp_state[512];

    struct hlist_node list_node;
} GCC_ATTR((aligned(16))) linked_pcb;

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} process_stack_frame;

typedef struct pcb {
    pid_t pid;
    char* name;

    physptr_t cr3;

    heap_t heap;

    uintptr_t opened_file_table;
    struct inode* cwd_i;

    kuid_t uid;
    kgid_t gid;

    struct hlist_head threads;

} pcb_t;

typedef struct tcb {

    tid_t tid;
    uint16_t state;

    pcb_t* process;

    uint64_t k_rsp;

    stack_t kernel_stack;
    stack_t user_stack;

    uintptr_t fs_base;
    uintptr_t gs_base;

    uint8_t fp_state[512];

    struct hlist_node list_node;

} tcb_t;


extern linked_pcb* _scheduler_current_process;
extern struct hlist_head _scheduler_process_list_head;
extern uint8_t task_switching_flag;

int scheduler_init();

void* sched_next_process_core(uint64_t saved_rsp);

linked_pcb* new_pcb(physptr_t page_dir, const char* name, uint64_t* rsp,
    stack_t k_stack, stack_t us_stack, uint16_t starting_state);


int kill_ktask(linked_pcb* pcb);


void _build_kernel_stack_frame(uint64_t* stack_top, uint64_t entry);
void _build_user_stack_frame(uint64_t** stack_top, uint64_t entry,
    uint64_t user_rsp, uint16_t cs, uint16_t ss);

linked_pcb* ktask_start(void* entry, char* name);
linked_pcb* us_task_start(void* entry, char* name, physptr_t page_dir,
                char** argv, char** envp);
void enable_scheduler();
void disable_scheduler();

void yield_core(uintptr_t sp);
void _yield();

void _sched_next_process();
void _log_all_processes();

