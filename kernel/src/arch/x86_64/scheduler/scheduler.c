#include "scheduler.h"
#include "arch/vmm.h"
#include "arch/x86_64/cpu/gdt.h"

#include "debug/Logger.h"
#include "defines/container_of.h"
#include "defines/err_codes.h"
#include "drivers/drivers.h"
#include "memops.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include "string/string.h"

#include <stdint.h>

HLIST_HEAD(_scheduler_process_list_head);

uint16_t process_list_depth = 1;
Linked_PCB_t* _scheduler_current_process = 0;

#define _PID_BITMAP_SIZE (MAX_PID / 64)

uint8_t task_switching_flag = 0;

uint64_t pid_bitmap[_PID_BITMAP_SIZE] = {[0 ... _PID_BITMAP_SIZE-1] = 0xFFFFFFFFULL};

pid_t _get_unused_pid() {
    for (int i = 0; i < _PID_BITMAP_SIZE; i++) {
        uint64_t* current = &pid_bitmap[i];

        if (!*current) continue;

        for (int j = 0; j < 64; j++) {
            if (*current & (1ULL << j)) {
                *current &= ~(1ULL << j);
                return (pid_t)(i * 64 + j);
            }
        }
    }
    return (pid_t)-1;
}

void _free_pid(pid_t pid) {
    uint16_t index = pid / 64;
    uint8_t bit = pid % 64;

    if (index >= _PID_BITMAP_SIZE) return;

    pid_bitmap[index] |= (1ULL << bit);
}

Linked_PCB_t* new_pcb(physptr_t page_dir, const char* name, uint64_t* rsp, Stack_t k_stack, Stack_t us_stack) {
    Linked_PCB_t* new_pcb = (Linked_PCB_t*)kmalloc(sizeof(Linked_PCB_t));
    if (!new_pcb) return NULL;

    new_pcb->pid = _get_unused_pid();
    if (new_pcb->pid < 0) return NULL;

    new_pcb->name = strdup(name);
    new_pcb->state = PCB_STATE_RUNNING;

    new_pcb->user_stack = us_stack;
    new_pcb->kernel_stack = k_stack;
    new_pcb->k_rsp = *rsp;

    new_pcb->cr3 = page_dir;

    new_pcb->list_node.next = NULL;

    if (!_scheduler_process_list_head.first) {
        _scheduler_process_list_head.first = &new_pcb->list_node;
    } else {
        hlist_add_head(&new_pcb->list_node, &_scheduler_process_list_head);
    }

    return new_pcb;
}

int kill_ktask(Linked_PCB_t* pcb) {
    if (!pcb) return -1;

    Sys_log("K Process %u (%s) exited with err:%d\n", pcb->pid, pcb->name, pcb->exit_code);

    hlist_del(&pcb->list_node);
    _free_pid(pcb->pid);

    pmm_free_pages(ADDR_TO_PAGE(pcb->kernel_stack.top), ADDR_TO_PAGE(pcb->kernel_stack.size));
    pmm_free_pages(ADDR_TO_PAGE(pcb->user_stack.top), ADDR_TO_PAGE(pcb->user_stack.size));

    kfree(pcb);
    process_list_depth--;

    return 0;
}

// REGISTER_DRIVER_CORE(scheduler, scheduler_init);

int scheduler_init() {
    pid_bitmap[0] &= ~(1ULL << 0);

    

    uint64_t tmp = 0x200000;

    new_pcb(kernel_pagedir_phys, "Kernel", &tmp,
        (Stack_t){0x200000, 0x1FF000, DEFAULT_STACK_PAGE_BYTES},
        (Stack_t){0});

    _scheduler_current_process =
        container_of(_scheduler_process_list_head.first, Linked_PCB_t, list_node);

    enable_scheduler();
    return 0;
}

void enable_scheduler() { task_switching_flag = 1; }
void disable_scheduler() { task_switching_flag = 0; }

void _setup_user_stack_sched_frame(void* us_stack_top, void* k_stack_top, uint64_t entry, uint64_t* out_rsp) {
    ProcessStackFrame* frame =
        (ProcessStackFrame*)((uint8_t*)k_stack_top - sizeof(ProcessStackFrame));

    memset(frame, 0, sizeof(ProcessStackFrame));

    frame->rip = entry;
    frame->cs = USER_CODE_SEGMENT;
    frame->rflags = 0x202;
    frame->userrsp = (uint64_t)us_stack_top;
    frame->ss = USER_DATA_SEGMENT;

    *out_rsp = (uint64_t)frame;
}

void _setup_kernel_stack_sched_frame(void* stack_top, uint64_t entry, uint64_t* out_rsp) {
    KProcessStackFrame* frame =
        (KProcessStackFrame*)((uint8_t*)stack_top - sizeof(KProcessStackFrame));

    memset(frame, 0, sizeof(KProcessStackFrame));

    frame->rip = entry;
    frame->cs = KERNEL_CODE_SEGMENT;
    frame->rflags = 0x202;

    *out_rsp = (uint64_t)frame;
}

Linked_PCB_t* us_task_start(void* entry, char* name, physptr_t page_dir) {
    void* k_stack = (void*)PAGE_TO_ADDR(pmm_alloc_pages(DEFAULT_STACK_PAGE_AMOUNT));
    RET_IF(!k_stack, 0);

    page_index us_stack_pages = pmm_alloc_pages(DEFAULT_STACK_PAGE_AMOUNT);
    RET_IF(!us_stack_pages, 0);

    uintptr_t us_stack_bott = PAGE_TO_ADDR(us_stack_pages);
    Sys_Fatal("not impl");
    for(;;);
    // pd_init(&page_dir);

    // pd_map_page(&page_dir,
    //     STACK_UPPER_USPACE_ADDR - DEFAULT_STACK_PAGE_BYTES,
    //     us_stack_pages, 1, 1, 1);

    // uint64_t out_rsp;

    // _setup_user_stack_sched_frame(
    //     (void*)(us_stack_bott + DEFAULT_STACK_PAGE_BYTES),
    //     k_stack + DEFAULT_STACK_PAGE_BYTES,
    //     (uint64_t)entry,
    //     &out_rsp
    // );

    // return new_pcb(&page_dir, name, &out_rsp,
    //     (Stack_t){(uintptr_t)k_stack, (uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES, DEFAULT_STACK_PAGE_BYTES},
    //     (Stack_t){us_stack_bott, us_stack_bott + DEFAULT_STACK_PAGE_BYTES, DEFAULT_STACK_PAGE_BYTES});
}

Linked_PCB_t* ktask_start(void* entry, char* name) {
    void* stack = (void*)PAGE_TO_ADDR(page_kalloc(DEFAULT_STACK_PAGE_AMOUNT, PTE_WRITABLE));
    RET_IF(!stack, 0);

    uint64_t out_rsp;

    _setup_kernel_stack_sched_frame(
        stack + DEFAULT_STACK_PAGE_BYTES,
        (uint64_t)entry,
        &out_rsp
    );

    return new_pcb(kernel_pagedir_phys, name, &out_rsp,
        (Stack_t){(uintptr_t)stack, (uintptr_t)stack + DEFAULT_STACK_PAGE_BYTES, DEFAULT_STACK_PAGE_BYTES},
        (Stack_t){0});
}

void* sched_next_process_core(uint64_t saved_rsp) {
    Linked_PCB_t* current = _scheduler_current_process;
    current->k_rsp = saved_rsp;

    struct hlist_node* next_node = current->list_node.next;

get_next:
    if (!next_node)
        next_node = _scheduler_process_list_head.first;

    Linked_PCB_t* next = container_of(next_node, Linked_PCB_t, list_node);

    if (next->state & PCB_STATE_ZOMBIE) {
        next_node = next->list_node.next;
        kill_ktask(next);
        goto get_next;
    }

    _scheduler_current_process = next;

    __asm__ volatile ("mov %0, %%cr3" :: "r"(next->cr3) : "memory");

    setTss_sp(next->k_rsp);

    return (void*)next->k_rsp;
}

extern void _ret_to_next_process(void* rsp);

void yield_core(uintptr_t sp) {
    _ret_to_next_process(sched_next_process_core(sp));
}

void _yield() {
    asm volatile (
        "int $0x80"
        :
        : "a"(158)
        : "memory"
    );
}