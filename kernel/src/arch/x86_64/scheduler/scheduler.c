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

#include <stddef.h>
#include <stdint.h>



HLIST_HEAD(_scheduler_process_list_head);

uint16_t process_list_depth = 1;
linked_pcb* _scheduler_current_process = 0;

#define _PID_BITMAP_SIZE (MAX_PID / 64)

uint8_t task_switching_flag = 0;

uint64_t pid_bitmap[_PID_BITMAP_SIZE] = {[0 ... _PID_BITMAP_SIZE-1] = 0xFFFFFFFFULL};

static size_t scheduler_lock = 0;
#define SCHED_LOCK_BIT 0

pid_t _get_unused_pid() {
    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    pid_t result = (pid_t)-1;

    for (int i = 0; i < _PID_BITMAP_SIZE; i++) {
        uint64_t* current = &pid_bitmap[i];

        if (!*current) continue;

        for (int j = 0; j < 64; j++) {
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

void _log_all_processes() {
    Sys_log("process list\n");
    struct hlist_node *n = _scheduler_process_list_head.first;
    while (n) {
        linked_pcb *p = container_of(n, linked_pcb, list_node);
        Sys_log("  pid=%-4u  state=0x%02x  name=%s\n",
                (unsigned)p->pid, (unsigned)p->state, p->name);
        n = n->next;
    }
    Sys_log("\n");
}







linked_pcb* new_pcb(physptr_t page_dir, const char* name, uint64_t* rsp,
                      stack_t k_stack, stack_t us_stack) {
    linked_pcb* new_pcb = (linked_pcb*)kmalloc(sizeof(linked_pcb));
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
    if (!pcb) return -1;

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


int scheduler_init() {
    Sys_Info("Initing PIT\n");
    pit_init();
    Sys_Info("Initing Scheduler\n");
    pid_bitmap[0] &= ~(1ULL << 0);

    

    uint64_t tmp = 0x200000;

    new_pcb(kernel_pagedir_phys, "Kernel", &tmp,
        (stack_t){0x200000, 0x1FF000, DEFAULT_STACK_PAGE_BYTES},
        (stack_t){0});

    _scheduler_current_process =
        container_of(_scheduler_process_list_head.first, linked_pcb, list_node);

    enable_scheduler();
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
        (stack_t){0}
    );
    return res;
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

linked_pcb* us_task_start(void* entry, char* name, physptr_t page_dir) {
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


    uintptr_t us_stack_bott = us_stack_pages;   // no PAGE_TO_ADDR

    for (int i = 0; i < DEFAULT_STACK_PAGE_AMOUNT; i++) {
        uintptr_t page_addr = us_stack_bott + i * PAGE_SIZE_4K;
        map_4k(p_dir, STACK_UPPER_USPACE_ADDR - (DEFAULT_STACK_PAGE_BYTES - i * PAGE_SIZE_4K),
            page_addr, PTE_WRITABLE | PTE_NX | PTE_USER | PTE_LOCAL_OWNED);   // no ADDR_TO_PAGE
    }
    
    
    uint64_t* stack_top = (uint64_t*)(k_stack + DEFAULT_STACK_PAGE_BYTES);
    
    _build_user_stack_frame(
        &stack_top,
        (uint64_t)entry,
        STACK_UPPER_USPACE_ADDR,
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
        }
    );

    




    physptr_t heap_pages = pmm_alloc_pages(256 / (PAGE_SIZE_4K/sizeof(file)));
    if(!heap_pages) {
        Sys_Error("task '%s': unable to allocate user heap pages\n", name);
        return NULL;
    }

    map_4k_pages(
        PHYS_2_HHDM(res->cr3),
        DEFAULT_USER_HEAP_START, heap_pages,
        DEFAULT_USER_HEAP_PAGES, PTE_USER | PTE_WRITABLE | PTE_LOCAL_OWNED
    );
    
    res->heap.bottom = DEFAULT_USER_HEAP_START;
    res->heap.top = DEFAULT_USER_HEAP_END;
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



    Sys_Success("task '%s': created sucesssfully..?\n", name);
    return res;
}
    
    
    
    
    
    
    
    






void* sched_next_process_core(uint64_t saved_rsp) {
    linked_pcb* current = _scheduler_current_process;
    current->k_rsp = saved_rsp;

#if SCHEDULER_PROC_LIST_DEBUG
    static uint32_t _tick = 0;
    if (++_tick >= SCHED_LOG_INTERVAL) {
        _tick = 0;
        _log_all_processes();
    }
#endif

#if SCHEDULER_TICK_SERIAL_DEBUG
    serial_write_string("exiting proc ");
    serial_log_hex("",current->pid);
    serial_write_char('\n');
#endif
    struct hlist_node* next_node = current->list_node.next;

get_next:
    if (!next_node)
        next_node = _scheduler_process_list_head.first;

    linked_pcb* next = container_of(next_node, linked_pcb, list_node);
#if SCHEDULER_TICK_SERIAL_DEBUG
    serial_write_string("running proc ");
    serial_log_hex("",next->pid);
    serial_write_char('\n');
#endif
    if (next->state & PCB_STATE_ZOMBIE) {
        next_node = next->list_node.next;
        if(next->cr3 == kernel_pagedir_phys) kill_ktask(next);
        else kill_us_task(next);
        goto get_next;
    }

    _scheduler_current_process = next;

    __asm__ volatile ("mov %0, %%cr3" :: "r"(next->cr3) : "memory");

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
