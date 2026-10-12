#pragma once
#include "arch/vmm.h"
#include "asm/asm.h"
#include "defines/compiler_defs.h"
#include "defines/lists.h"
#include "memory/pmm.h"
#include "vfs/vfs.h"


#include <stdint.h>



#define MAX_TID                     16384


#define ID_TAG_BIT   63
#define ID_TAG_MASK  (1ULL << ID_TAG_BIT)


#define DEFAULT_STACK_PAGE_AMOUNT   128
#define DEFAULT_STACK_PAGE_BYTES    (DEFAULT_STACK_PAGE_AMOUNT << 12)

#define STACK_LOWER_USPACE_ADDR     0x00007F0000000000ULL
#define STACK_UPPER_USPACE_ADDR     0x0000800000000000ULL

#define STACK_USPACE_SIZE           \
        (STACK_UPPER_USPACE_ADDR - STACK_LOWER_USPACE_ADDR)

#define STACK_MAX_THREADS           \
        (STACK_USPACE_SIZE / DEFAULT_STACK_PAGE_BYTES)



        
#define DEFAULT_FILE_TABLE_PAGES    4 //256 entries if file struct =64 bytes
#define DEFAULT_FILE_TABLE_START    0x00007E0000000000 //2TiB under stacks start

#define DEFAULT_FILE_TABLE_ENTRIES  DEFAULT_FILE_TABLE_PAGES * (PAGE_SIZE_4K/ sizeof(file))


#define DEFAULT_USER_HEAP_PAGES     16 //64KB
#define DEFAULT_USER_HEAP_START     0x0000600000000000 //32TiB under stack start/upper
#define DEFAULT_USER_HEAP_END       (0x0000600000000000 + \
                DEFAULT_USER_HEAP_PAGES * PAGE_SIZE_4K)

                
#define USER_ARGS_MAX_PAGES         16
#define USER_ARGS_USPACE_ADDR       ((DEFAULT_USER_HEAP_START - PAGE_SIZE_4K*(USER_ARGS_MAX_PAGES+1)))



typedef short pid_t;
typedef short tid_t;


typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} process_stack_frame;



#define PCB_STATE_RUNNABLE     0x0001
#define PCB_STATE_STARTING_UP  0x0002
#define PCB_STATE_ZOMBIE       0x0004
#define PCB_STATE_DEAD         0x0008
struct pcb;
typedef struct pcb {
    pid_t pid;
    char* name;
    
    uint16_t state;
    int exit_code;
    
    physptr_t cr3;
    
    heap_t heap;
    
    uintptr_t opened_file_table;
    physptr_t opened_file_table_phys;
    struct inode* cwd_i;
    
    kuid_t uid;
    kgid_t gid;
    
    struct hlist_node proclist_node;
    
    struct hlist_head threads;
    
    struct pcb* parent;
    struct hlist_head childs;    
    struct hlist_node siblings_list_node;

    uint64_t signals_bitmap;
    void* signal_handlers_array[64];
    
} pcb_t;

#define TCB_STATE_RUNNABLE     0x0001
#define TCB_STATE_STARTING_UP  0x0002
#define TCB_STATE_ZOMBIE       0x0004
#define TCB_STATE_DEAD         0x0008
typedef struct tcb {

    tid_t tid;
    uint16_t state;
    int exit_code;

    pcb_t* process;

    uint64_t k_rsp;

    stack_t kernel_stack;
    stack_t user_stack;

    uintptr_t fs_base;
    uintptr_t gs_base;

    GCC_ATTR((aligned(16))) uint8_t fp_state[512];

    struct hlist_node proc_list_node;
    struct hlist_node shed_list_node;


    uint64_t signal_mask;
    // #error add listeners/etc for waitpid

}GCC_ATTR((aligned(16))) tcb_t;



extern struct hlist_head _scheduler_processes_list_head;
extern uint8_t task_switching_flag;

extern struct hlist_head scheduler_threads;

extern pcb_t* current_process;
extern tcb_t* current_thread;

#define SCHED_LOCK_BIT 0
extern size_t scheduler_lock;

extern uint16_t process_list_depth;

extern pcb_t* kernel_proc;

void _free_tid(tid_t tid);
tid_t _get_unused_tid();




typedef enum { ID_NONE, ID_PROC, ID_THREAD } id_kind_t;
extern void* id_table[];

static inline void id_table_set_proc(pid_t pid, pcb_t* proc) {
    id_table[pid] = (void*)((uintptr_t)proc & ~ID_TAG_MASK); // clear bit 63 = "proc"
}
static inline void id_table_set_thread(tid_t tid, tcb_t* t) {
    id_table[tid] = (void*)t; 
}
static inline void id_table_clear(uint16_t id) {
    id_table[id] = NULL;
}

void* id_lookup(uint16_t id, id_kind_t* kind_out);




int scheduler_init();

void* sched_next_process_core(uint64_t saved_rsp);
void _sched_next_process();




//constructors
pcb_t* new_usproc(pcb_t* parent, void* entry, char* name, physptr_t page_dir,
    char** argv, char** envp);
    
tcb_t* new_thread(pcb_t* proc, void* entry, uint16_t starting_state);
tcb_t* ktask_start(void* entry, char* name);

//stack frames
void _build_kernel_stack_frame(uint64_t* stack_top, uint64_t entry);
void _build_user_stack_frame(uint64_t** stack_top, uint64_t entry,
    uint64_t user_rsp, uint16_t cs, uint16_t ss);
        
// killing funcs
int kill_ktask(tcb_t* t);

    //userspace exits
    void usthread_exit(tcb_t* t, int exit_code);
    void usproc_exit(pcb_t* proc, int exit_code);

    //userspace reaps
    int  reap_us_thread(tcb_t* t);
    int  reap_us_proc(pcb_t* proc);
    void reap_us_proc_threads(pcb_t* proc);


//misc/debug
void enable_scheduler();
void disable_scheduler();

void yield_core(uintptr_t sp);
void _yield();


void _log_all_processes();
void _log_process_tree();

const char* _tcb_state_str(uint16_t state);