#include "arch/locks.h"
#include "arch/vmm.h"
#include "arch/x86_64/cpu/gdt.h"
#include "asm/asm.h"
#include "config.h"
#include "debug/Logger.h"
#include "defines/container_of.h"
#include "defines/err_codes.h"
#include "defines/lists.h"
#include "drivers/pit/pit.h"
#include "memops.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include "mm/vmm_arch.h"
#include "scheduler.h"
#include "string/string.h"
#include "vfs/vfs.h"

#include "defines/auxv.h"
#include <stddef.h>
#include <stdint.h>




HLIST_HEAD(_scheduler_process_list_head);

uint16_t process_list_depth = 1;
linked_pcb* _scheduler_current_process = 0;

pcb_t* current_process;
tcb_t* current_thread;

#define _PID_BITMAP_SIZE (MAX_PID / 64)

uint8_t task_switching_flag = 0;

uint64_t pid_bitmap[_PID_BITMAP_SIZE] = {[0 ... _PID_BITMAP_SIZE-1] = 0xFFFFFFFFULL};

static size_t scheduler_lock = 0;
#define SCHED_LOCK_BIT 0

bool pid0_used = false;

pid_t _get_unused_pid() {
    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    pid_t result = (pid_t)-1;

    for (int i = 0; i < _PID_BITMAP_SIZE; i++) {
        uint64_t* current = &pid_bitmap[i];


        if (!*current) continue;

        for (int j = 0; j < 64; j++) {
            
            if (pid0_used && i == 0 && j == 0) continue;

            if (*current & (1ULL << j)) {
                *current &= ~(1ULL << j);
                result = (pid_t)(i * 64 + j);
                goto done;
            }
        }
    }

done:
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);
    return result;
}

void _free_pid(pid_t pid) {
    uint16_t index = pid / 64;
    uint8_t bit = pid % 64;

    if (index >= _PID_BITMAP_SIZE) return;

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    pid_bitmap[index] |= (1ULL << bit);
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);
}








#define SCHED_LOG_INTERVAL 2000U

void _log_all_processes(void) {
    Sys_log("process list\n");
    Sys_log("-----------------------------------------------------------------------------------------------\n");
    

    struct hlist_node *n = _scheduler_process_list_head.first;
    while (n) {
        linked_pcb *p = container_of(n, linked_pcb, list_node);

        Sys_Info(
            "pid=%u "
            "state=0x%04x "
            "name=%s "
            "cr3=0x%016llx "
            "k_rsp=0x%016llx "
            "heap=[0x%016llx-0x%016llx] "
            "kstack=[0x%016llx-0x%016llx] "
            "ustack=[0x%016llx-0x%016llx] "
            "files=%p "
            "cwd=%p "
            "exit=%d "
            "list_node=%p\n\n",
            (unsigned)p->pid,
            (unsigned)p->state,
            p->name ? p->name : "(null)",
            (unsigned long long)p->cr3,
            (unsigned long long)p->k_rsp,
        
            (unsigned long long)p->heap.lower,
            (unsigned long long)p->heap.higher,
        
            (unsigned long long)p->kernel_stack.bottom,
            (unsigned long long)p->kernel_stack.top,
        
            (unsigned long long)p->user_stack.bottom,
            (unsigned long long)p->user_stack.top,
        
            (void *)p->opened_file_table,
            (void *)p->cwd_i,
            p->exit_code,
            (void *)&p->list_node
        );
        n = n->next;
    }

    Sys_log("-----------------------------------------------------------------------------------------------\n\n");
}






linked_pcb* new_pcb(physptr_t page_dir, const char* name, uint64_t* rsp,
                      stack_t k_stack, stack_t us_stack, uint16_t starting_state) {
    linked_pcb* new_pcb = (linked_pcb*)struct_kmalloc_align(linked_pcb);
    if (!new_pcb) return NULL;

    pid_t pid = _get_unused_pid();
    if (pid < 0) {
        kfree(new_pcb);
        return NULL;
    }

    new_pcb->pid = pid;
    new_pcb->name = strdup(name);
    new_pcb->state = PCB_STATE_RUNNING;
    new_pcb->user_stack = us_stack;
    new_pcb->kernel_stack = k_stack;
    new_pcb->k_rsp = *rsp;
    new_pcb->cr3 = page_dir;
    new_pcb->list_node.next = NULL;

    new_pcb->state = starting_state;

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    if (!_scheduler_process_list_head.first) {
        _scheduler_process_list_head.first = &new_pcb->list_node;
    } else {
        hlist_add_head(&new_pcb->list_node, &_scheduler_process_list_head);
    }

    process_list_depth++;

    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    return new_pcb;
}

int kill_ktask(linked_pcb* pcb) {
    if (!pcb) return -1;

    Sys_log("ktask %u (%s) exited with code:%d\n",
            pcb->pid, pcb->name, pcb->exit_code);

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    hlist_del(&pcb->list_node);
    process_list_depth--;

    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    _free_pid(pcb->pid);

    pmm_free_pages(ADDR_TO_PAGE(pcb->kernel_stack.top),
                   ADDR_TO_PAGE(pcb->kernel_stack.size));

    kfree(pcb);

    return 0;
}

int kill_us_task(linked_pcb* pcb) {
    if (!pcb) return -1;typedef struct pcb {
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

    Sys_log("user task %u (%s) exited with code:%d\n",
            pcb->pid, pcb->name, pcb->exit_code);

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    hlist_del(&pcb->list_node);
    process_list_depth--;

    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    _free_pid(pcb->pid);

    page_kfree(
        pcb->kernel_stack.top,
        ADDR_TO_PAGE(pcb->kernel_stack.size)
    );

    pmm_free_pages(
        pcb->user_stack.top,
        ADDR_TO_PAGE(pcb->user_stack.size)
    );
    
    vmm_pagetable_free_owned(pcb->cr3);
    

    kfree(pcb->name);
    // hlist_del(&pcb->list_node);
    kfree(pcb);

    return 0;
}


static void reaper_ktask(void) {
    while (true) {
        linked_pcb* victim = NULL;

        acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
        struct hlist_node* n = _scheduler_process_list_head.first;
        while (n) {
            linked_pcb* p = container_of(n, linked_pcb, list_node);
            if (p->state & PCB_STATE_DEAD) {
                victim = p;
                break;
            }
            n = n->next;
        }
        release_lock(&scheduler_lock, SCHED_LOCK_BIT);

        if (!victim) {
            _yield();
            continue;
        }

        if (victim->cr3 == kernel_pagedir_phys)
            kill_ktask(victim);
        else
            kill_us_task(victim);
    }
}


int scheduler_init() {
    Sys_Info("Initing PIT\n");
    pit_init();
    Sys_Info("Initing Scheduler\n");

    
    uint64_t tmp = 0x200000;
    new_pcb(kernel_pagedir_phys, "Kernel", &tmp,
        (stack_t){0x200000, 0x1FF000, DEFAULT_STACK_PAGE_BYTES},
        (stack_t){0},
        PCB_STATE_RUNNING
    );
    
    _scheduler_current_process =
        container_of(_scheduler_process_list_head.first, linked_pcb, list_node);

    pid0_used = true;

    enable_scheduler();
// #fix teh damn reaper killing himself since ktask are shit now smh
    // ktask_start(reaper_ktask, "reaper");

    Sys_Success("Scheduler Inited\n");
    return 0;
}

void enable_scheduler() { task_switching_flag = 1; }
void disable_scheduler() { task_switching_flag = 0; }



void _build_kernel_stack_frame(uint64_t* stack_top, uint64_t entry) {
    uint64_t base = *stack_top;
    process_stack_frame* frame =
        (process_stack_frame*)(base - sizeof(process_stack_frame));

    memset(frame, 0, sizeof(process_stack_frame));

    frame->rip    = entry;
    frame->cs     = KERNEL_CODE_SEGMENT;
    frame->rflags = 0x202;
    frame->rsp    = base;
    frame->ss     = KERNEL_DATA_SEGMENT;

    *stack_top = (uint64_t)frame;
}


linked_pcb* ktask_start(void* entry, char* name) {

    void* stack = (void*)page_kalloc(DEFAULT_STACK_PAGE_AMOUNT+1, PTE_WRITABLE);
    RET_IF(!stack, NULL);
    uint64_t out_rsp = (uintptr_t)stack + DEFAULT_STACK_PAGE_BYTES;
    
    _build_kernel_stack_frame(
        &out_rsp,
        (uint64_t)entry  
    );
    // Sys_Warning("%p",stack);

    linked_pcb* res = new_pcb(kernel_pagedir_phys, name, &out_rsp,
        (stack_t){
            .bottom = (uintptr_t)stack,
            .top = (uintptr_t)stack + DEFAULT_STACK_PAGE_BYTES,

            .size = DEFAULT_STACK_PAGE_BYTES
        },
        (stack_t){0},
        PCB_STATE_RUNNING
    );
    return res;
}























static size_t _argv_len(char** arr) {
    size_t n = 0;
    if (arr) while (arr[n]) n++;
    return n;
}
static bool _setup_user_stack_args(void* k_stack_top, uint64_t u_stack_top,
                                    char** argv, char** envp,
                                    uint64_t* out_user_rsp) {
    size_t argc = _argv_len(argv);
    size_t envc = _argv_len(envp);

    size_t str_bytes = 0;
    for (size_t i = 0; i < argc; i++) str_bytes += strlen(argv[i]) + 1;
    for (size_t i = 0; i < envc; i++) str_bytes += strlen(envp[i]) + 1;

    size_t argc_bytes = sizeof(uint64_t);
    size_t argv_bytes = (argc + 1) * sizeof(uint64_t);
    size_t envp_bytes = (envc + 1) * sizeof(uint64_t);
    size_t auxv_bytes = 48 * sizeof(uint64_t); //margin becaus i'm lazy

    size_t fixed_bytes = argc_bytes + argv_bytes + envp_bytes + auxv_bytes;

    
    size_t unaligned = fixed_bytes + str_bytes;
    size_t pad = (16 - (unaligned % 16)) % 16;

    size_t total = fixed_bytes + pad + str_bytes;
    if (total > DEFAULT_STACK_PAGE_BYTES) return false; //i swear if i hit that...

    uint8_t* base     = (uint8_t*)k_stack_top - total;
    uint64_t base_uva = u_stack_top - total;
    memset(base, 0, total);

    uint64_t* k_argc      = (uint64_t*)base;
    uint64_t* k_argv_ptrs = (uint64_t*)(base + argc_bytes);
    uint64_t* k_envp_ptrs = (uint64_t*)((uint8_t*)k_argv_ptrs + argv_bytes);
    uint64_t* k_auxv      = (uint64_t*)((uint8_t*)k_envp_ptrs + envp_bytes);
    uint8_t*  k_strings   = (uint8_t*)k_auxv + auxv_bytes + pad;
    uint64_t  strings_uva = base_uva + (fixed_bytes + pad);

    size_t str_off = 0;
    for (size_t i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        memcpy(k_strings + str_off, argv[i], len);
        k_argv_ptrs[i] = strings_uva + str_off;
        str_off += len;
    }
    k_argv_ptrs[argc] = 0;

    for (size_t i = 0; i < envc; i++) {
        size_t len = strlen(envp[i]) + 1;
        memcpy(k_strings + str_off, envp[i], len);
        k_envp_ptrs[i] = strings_uva + str_off;
        str_off += len;
    }
    k_envp_ptrs[envc] = 0;

	size_t k = 0;
    k_auxv[k++] = AT_PAGESZ; k_auxv[k++] = PAGE_SIZE_4K;
    k_auxv[k++] = AT_RANDOM; k_auxv[k++] = u_stack_top-16;
    k_auxv[k++] = AT_NULL;   k_auxv[k++] = 0;

    *k_argc = argc;
    *out_user_rsp = base_uva;
    return true;
}

void _build_user_stack_frame(uint64_t** stack_top, uint64_t entry,
                        uint64_t user_rsp, uint16_t cs, uint16_t ss) {
    uint64_t base = (uint64_t)*stack_top;
    process_stack_frame* frame =
        (process_stack_frame*)(base - sizeof(process_stack_frame));

    memset(frame, 0, sizeof(process_stack_frame));

    frame->rip    = entry;
    frame->cs     = cs;
    frame->rflags = 0x202;
    frame->rsp    = user_rsp;
    frame->ss     = ss;

    *stack_top = (uint64_t*)frame;
}

linked_pcb* us_task_start(void* entry, char* name, physptr_t page_dir,
                           char** argv, char** envp) {
    Sys_Debug("Starting user task '%s'(cr3: %p)\n", name, (void*)page_dir);
    if (!page_dir) {
        Sys_Error("task '%s': no page directory provided\n", name);
        return NULL;
    }

    uint64_t* p_dir = PHYS_2_HHDM(page_dir);

    // map the kernel space manually
    for (int i = 255; i < 512; i++)
        p_dir[i] = ((uint64_t*)PHYS_2_HHDM(kernel_pagedir_phys))[i];

    void* k_stack = (void*)page_kalloc(DEFAULT_STACK_PAGE_AMOUNT + 1, PTE_WRITABLE);
    if (!k_stack) {
        Sys_Error("task '%s': unable to allocate kernel stack\n", name);
        return NULL;
    }

    page_index us_stack_pages = pmm_alloc_pages(DEFAULT_STACK_PAGE_AMOUNT);
    if (!us_stack_pages) {
        Sys_Error("task '%s': unable to allocate user stack pages\n", name);
        return NULL;
    }

    uintptr_t us_stack_bott = us_stack_pages;

    for (int i = 0; i < DEFAULT_STACK_PAGE_AMOUNT; i++) {
        uintptr_t page_addr = us_stack_bott + i * PAGE_SIZE_4K;
        map_4k(p_dir, STACK_UPPER_USPACE_ADDR - (DEFAULT_STACK_PAGE_BYTES - i * PAGE_SIZE_4K),
            page_addr, PTE_WRITABLE | PTE_NX | PTE_USER | PTE_LOCAL_OWNED);
    }

    
    void* us_stack_khost_top = (uint8_t*)PHYS_2_HHDM(us_stack_bott) + DEFAULT_STACK_PAGE_BYTES;

    uint64_t user_rsp;
    if (!_setup_user_stack_args(us_stack_khost_top, STACK_UPPER_USPACE_ADDR,
                                 argv, envp, &user_rsp)) {
        Sys_Error("task '%s': unable to set up argv/envp on user stack\n", name);
        return NULL;
    }

    uint64_t* stack_top = (uint64_t*)(k_stack + DEFAULT_STACK_PAGE_BYTES);

    _build_user_stack_frame(
        &stack_top,
        (uint64_t)entry,
        user_rsp,
        USER_CODE_SEGMENT, USER_DATA_SEGMENT
    );

    linked_pcb* res = new_pcb(
        page_dir,
        name,
        (uint64_t*)&stack_top,
        (stack_t){
            .bottom = (uintptr_t)k_stack,
            .top = (uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES,
            .size = DEFAULT_STACK_PAGE_BYTES
        },
        (stack_t){
            .bottom = us_stack_bott,
            .top = us_stack_bott + DEFAULT_STACK_PAGE_BYTES,
            .size = DEFAULT_STACK_PAGE_BYTES
        },
        PCB_STATE_STARTING
    );

    physptr_t heap_pages = pmm_alloc_pages(DEFAULT_USER_HEAP_PAGES);
    if(!heap_pages) {
        Sys_Error("task '%s': unable to allocate user heap pages\n", name);
        return NULL;
    }

    map_4k_pages(
        PHYS_2_HHDM(res->cr3),
        DEFAULT_USER_HEAP_START, heap_pages,
        DEFAULT_USER_HEAP_PAGES, PTE_USER | PTE_WRITABLE | PTE_LOCAL_OWNED
    );

    res->heap.lower = DEFAULT_USER_HEAP_START;
    res->heap.higher = DEFAULT_USER_HEAP_END;
    res->heap.size = DEFAULT_USER_HEAP_PAGES * PAGE_SIZE_4K;

    physptr_t ft_pages = pmm_alloc_pages(256 / (PAGE_SIZE_4K/sizeof(file)));
    if(!ft_pages) {
        Sys_Error("task '%s': unable to allocate user file table pages\n", name);
        return NULL;
    }

    map_4k_pages(
        PHYS_2_HHDM(res->cr3),
        DEFAULT_FILE_TABLE_START, ft_pages,
        DEFAULT_FILE_TABLE_PAGES, PTE_WRITABLE | PTE_LOCAL_OWNED
    );

    res->opened_file_table = DEFAULT_FILE_TABLE_START;

    res->fs_base = 0;
    res->gs_base = 0;

    
    memset(res->fp_state, 0, sizeof(res->fp_state));// to not blow up once switching
    *(uint32_t*)(res->fp_state + 24) = 0x1F80; // MXCSR def

    

    Sys_Success("task '%s': created sucesssfully..?\n", name);
    return res;
}
    
    
    
    
    
    
    
    






extern uint8_t fpu_enabled;
void* sched_next_process_core(uint64_t saved_rsp) {
    linked_pcb* current = _scheduler_current_process;
    current->k_rsp = saved_rsp;

    current->fs_base = rdmsr(MSR_FS_BASE);
    current->gs_base = rdmsr(MSR_GS_BASE);
    if (fpu_enabled) {
        fxsave_state(current->fp_state);
    }

#if SCHEDULER_PROC_LIST_DEBUG
    static uint32_t _tick = 0;
    if (++_tick >= SCHED_LOG_INTERVAL) {
        _tick = 0;
        _log_all_processes();
    }
#endif

#if SCHEDULER_TICK_SERIAL_DEBUG
static int tick_ctr = 0;
if(tick_ctr>=1000){
    serial_write_string("exiting proc ");
    serial_log_hex("",current->pid);
    serial_write_char('\n');
    
}
#endif

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    struct hlist_node* next_node = current->list_node.next;

get_next:
    if (!next_node)
        next_node = _scheduler_process_list_head.first;

    linked_pcb* next = container_of(next_node, linked_pcb, list_node);
#if SCHEDULER_TICK_SERIAL_DEBUG
if(tick_ctr>=1000){
    serial_write_string("running proc ");
    serial_log_hex("",next->pid);
    serial_write_char('\n');
    tick_ctr=0;
}
#endif
    if (!next || !(next->state & PCB_STATE_RUNNING)) {
        next_node = next->list_node.next;
        goto get_next;
    }

    _scheduler_current_process = next;

    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    __asm__ volatile ("mov %0, %%cr3" :: "r"(next->cr3) : "memory");
    
    wrmsr(MSR_FS_BASE, next->fs_base);
    wrmsr(MSR_GS_BASE, next->gs_base);
    if (fpu_enabled) {
        fxrstor_state(next->fp_state);
    }

    set_tss_sp(next->k_rsp);

    return (void*)next->k_rsp;
}

extern void _ret_to_next_process(void* rsp);

void yield_core(uintptr_t sp) {
    _ret_to_next_process(sched_next_process_core(sp));
}


void _kernel_yield();

void _yield() {
    _kernel_yield();
}