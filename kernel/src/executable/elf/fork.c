#include "fork.h"

#include "arch/locks.h"
#include "config.h"
#include "scheduler/scheduler.h"
#include "mm/vmm_arch.h"
#include "memory/pmm.h"
#include "memory/memory.h"
#include "defines/err_codes.h"
#include "arch/x86_64/cpu/gdt.h"
#include "debug/Logger.h"
#include "memops.h"
#include "string/string.h"
#include "timer/time.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


static inline uint64_t* _table_virt(uint64_t entry)
{
    return (uint64_t*)PHYS_2_HHDM(entry & PTE_ADDR_MASK);
}


static bool _fork_copy_user_tables(uint64_t* parent_pml4, uint64_t* child_pml4)
{
    for (int i4 = 0; i4 < 256; i4++) {
        if (!(parent_pml4[i4] & PTE_PRESENT)) continue;

        uint64_t* pdpt = _table_virt(parent_pml4[i4]);

        physptr_t child_pdpt_phys = pmm_alloc_pages_zeroed(1);
        if (!child_pdpt_phys) return false;
        uint64_t* child_pdpt = PHYS_2_HHDM(child_pdpt_phys);
        child_pml4[i4] = child_pdpt_phys | (parent_pml4[i4] & ~PTE_ADDR_MASK);

        for (int i3 = 0; i3 < PT_ENTRIES; i3++) {
            if (!(pdpt[i3] & PTE_PRESENT)) continue;

            uint64_t* pd = _table_virt(pdpt[i3]);

            physptr_t child_pd_phys = pmm_alloc_pages_zeroed(1);
            if (!child_pd_phys) return false;
            uint64_t* child_pd = PHYS_2_HHDM(child_pd_phys);
            child_pdpt[i3] = child_pd_phys | (pdpt[i3] & ~PTE_ADDR_MASK);

            for (int i2 = 0; i2 < PT_ENTRIES; i2++) {
                if (!(pd[i2] & PTE_PRESENT)) continue;

                uint64_t* pt = _table_virt(pd[i2]);

                physptr_t child_pt_phys = pmm_alloc_pages_zeroed(1);
                if (!child_pt_phys) return false;
                uint64_t* child_pt = PHYS_2_HHDM(child_pt_phys);
                child_pd[i2] = child_pt_phys | (pd[i2] & ~PTE_ADDR_MASK);

                for (int i1 = 0; i1 < PT_ENTRIES; i1++) {
                    uint64_t pte = pt[i1];
                    if (!(pte & PTE_PRESENT)) continue;
                
                    if (pte & PTE_LOCAL_OWNED) {
                        physptr_t src_phys = pte & PTE_ADDR_MASK;
                        physptr_t dst_phys = pmm_alloc_pages(1);
                        if (!dst_phys) return false;
                        memcpy(PHYS_2_HHDM(dst_phys), PHYS_2_HHDM(src_phys), PAGE_SIZE_4K);
                        child_pt[i1] = dst_phys | (pte & ~PTE_ADDR_MASK);
                    } else {
                        child_pt[i1] = pte;
                    }
                }
            }
        }
    }

    return true;
}


pid_t fork_process(tcb_t* parent_thread, uint64_t rsp)
{
    if (!parent_thread || !parent_thread->process) {
        Sys_Error("fork: no parent thread\n");
        return -E_INVAL;
    }

    pcb_t* parent = parent_thread->process;

    if (!parent->cr3 || parent->cr3 == kernel_pagedir_phys) {
        Sys_Error("fork: can only fork a user task\n");
        return -E_INVAL;
    }

    process_stack_frame* parent_frame = (process_stack_frame*)rsp;

#if FORK_DEBUG
    Sys_Debug("fork: parent pid=%u cr3=%p rsp=%p\n",
              parent->pid, (void*)parent->cr3, (void*)rsp);
#endif

    physptr_t child_pml4_phys = pmm_alloc_pages_zeroed(1);
    RET_IF(!child_pml4_phys, -E_NOMEM);

    uint64_t* parent_pml4 = PHYS_2_HHDM(parent->cr3);
    uint64_t* child_pml4  = PHYS_2_HHDM(child_pml4_phys);

    for (int i = 256; i < 512; i++)
        child_pml4[i] = ((uint64_t*)PHYS_2_HHDM(kernel_pagedir_phys))[i];

    if (!_fork_copy_user_tables(parent_pml4, child_pml4)) {
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }

#if FORK_DEBUG
    Sys_Debug("fork: child pml4=%p ready\n", (void*)child_pml4_phys);
#endif

    // child pcb
    pcb_t* child = (pcb_t*)struct_kmalloc_align(pcb_t);
    if (!child) {
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }
    memset(child, 0, sizeof(pcb_t));

    tid_t child_pid = _get_unused_tid();
    if (child_pid < 0) {
        kfree(child);
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }

    child->pid  = child_pid;
    child->name = strdup(parent->name);
    child->cr3  = child_pml4_phys;
    child->uid  = parent->uid;
    child->gid  = parent->gid;
    child->threads.first = NULL;
    child->childs.first  = NULL;
    child->parent = parent;

    child->heap = parent->heap;
    child->opened_file_table = parent->opened_file_table;
    child->cwd_i = parent->cwd_i;

    tcb_t* child_thread = (tcb_t*)struct_kmalloc_align(tcb_t);
    if (!child_thread) {
        _free_tid(child->pid);
        kfree(child->name);
        kfree(child);
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }
    memset(child_thread, 0, sizeof(tcb_t));
    *(uint32_t*)(child_thread->fp_state + 24) = 0x1F80; // MXCSR def
    child_thread->process = child;

    void* k_stack = (void*)page_kalloc(DEFAULT_STACK_PAGE_AMOUNT + 1, PTE_WRITABLE);
    if (!k_stack) {
        kfree(child_thread);
        _free_tid(child->pid);
        kfree(child->name);
        kfree(child);
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }

    uint64_t* stack_top =
        (uint64_t*)((uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES);

    process_stack_frame* child_frame =
        (process_stack_frame*)((uintptr_t)stack_top - sizeof(process_stack_frame));

    *child_frame = *parent_frame;
    child_frame->rax = 0; // fork return 0 to child 

    child_thread->kernel_stack.top    = (uintptr_t)k_stack;
    child_thread->kernel_stack.bottom = (uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES;
    child_thread->kernel_stack.size   = DEFAULT_STACK_PAGE_BYTES;
    child_thread->k_rsp = (uint64_t)child_frame;

    child_thread->user_stack = parent_thread->user_stack;

    child_thread->tid = _get_unused_tid();
    if (child_thread->tid < 0) {
        page_kfree((uintptr_t)k_stack, DEFAULT_STACK_PAGE_AMOUNT + 1);
        kfree(child_thread);
        _free_tid(child->pid);
        kfree(child->name);
        kfree(child);
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    child->state = PCB_STATE_RUNNABLE;
    hlist_add_head(&child->siblings_lsist_node, &parent->childs);
    id_table_set_proc(child->pid, child);
    id_table_set_thread(child_thread->tid, child_thread);

    hlist_add_head(&child_thread->proc_list_node, &child->threads);
    hlist_add_head(&child->proclist_node, &_scheduler_processes_list_head);
    process_list_depth++;
    hlist_add_head(&child_thread->shed_list_node, &scheduler_threads);

    release_lock(&scheduler_lock, SCHED_LOCK_BIT);
    
#if FORK_DEBUG
    Sys_Success("fork: pid %u -> child pid %u\n", parent->pid, child->pid);
#endif
Sys_Warning(
    "fork frames: parent rip=%016llx rsp=%016llx rflags=%016llx, "
    "child rip=%016llx rsp=%016llx rflags=%016llx\n",
    (unsigned long long)parent_frame->rip,
    (unsigned long long)parent_frame->rsp,
    (unsigned long long)parent_frame->rflags,
    (unsigned long long)child_frame->rip,
    (unsigned long long)child_frame->rsp,
    (unsigned long long)child_frame->rflags
);
sleep_ms(100);
    child_thread->state = TCB_STATE_STARTING_UP;

    return child->pid;
}