#include "fork.h"

#include "config.h"
#include "scheduler/scheduler.h"
#include "mm/vmm_arch.h"
#include "memory/pmm.h"
#include "memory/memory.h"
#include "defines/err_codes.h"
#include "arch/x86_64/cpu/gdt.h"
#include "debug/Logger.h"
#include "memops.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


static inline uint64_t* _table_virt(uint64_t entry)
{
    return (uint64_t*)PHYS_2_HHDM(entry & PTE_ADDR_MASK);
}

/*
 * Walk the parent's user-half (PML4[0..254]) page tables and build an
 * identical tree of page tables for the child. Any leaf PTE marked
 * PRESENT | LOCAL_OWNED gets a brand-new physical page with the parent's
 * page contents copied in, mapped at the same virtual address with the
 * same flags. Non-owned present entries are left unmapped in the child.
 *
 * Every intermediate table (PDPT/PD/PT) is freshly allocated for the child
 * even if the parent's corresponding table has no owned leaves in it --
 * simpler to reason about than detecting "empty" subtrees.
 */
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
                    if (!(pte & PTE_LOCAL_OWNED)) continue; // skip non-owned

                    physptr_t src_phys = pte & PTE_ADDR_MASK;
                    physptr_t dst_phys = pmm_alloc_pages(1);
                    if (!dst_phys) return false;

                    memcpy(PHYS_2_HHDM(dst_phys), PHYS_2_HHDM(src_phys), PAGE_SIZE_4K);

                    child_pt[i1] = dst_phys | (pte & ~PTE_ADDR_MASK);
                }
            }
        }
    }

    return true;
}

/*
 * fork_process: clone a user task.
 *
 * parent: the pcb of the task being forked (need not be
 *         _scheduler_current_process, though normally will be).
 * rsp:    the parent's saved stack pointer, with a process_stack_frame
 *         already pushed at the top -- the same convention used by
 *         sched_next_process_core()'s saved_rsp argument. This frame is
 *         copied verbatim into the child's new kernel stack, with rax
 *         forced to 0 so the child sees fork() return 0 when resumed.
 *
 * The parent's own rax (child pid) must be set by the caller (e.g. your
 * syscall dispatcher) using the same rsp/frame after this call returns --
 * fork_process() does not modify the parent's frame.
 *
 * Returns the child's pid on success, or a negative -E_* code on failure.
 * All partially-built state is cleaned up on any failure path.
 */
pid_t fork_process(linked_pcb* parent, uint64_t rsp)
{
    if (!parent || !parent->cr3 || parent->cr3 == kernel_pagedir_phys) {
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

    /* kernel half is identical/shared across every address space */
    for (int i = 255; i < 512; i++)
        child_pml4[i] = ((uint64_t*)PHYS_2_HHDM(kernel_pagedir_phys))[i];

    if (!_fork_copy_user_tables(parent_pml4, child_pml4)) {
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }

#if FORK_DEBUG
    Sys_Debug("fork: child pml4=%p ready\n", (void*)child_pml4_phys);
#endif

    void* k_stack = (void*)page_kalloc(DEFAULT_STACK_PAGE_AMOUNT + 1, PTE_WRITABLE);
    if (!k_stack) {
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }

    uint64_t* stack_top =
        (uint64_t*)((uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES);

    process_stack_frame* child_frame =
        (process_stack_frame*)((uintptr_t)stack_top - sizeof(process_stack_frame));

    *child_frame = *parent_frame;
    child_frame->rax = 0; /* fork() returns 0 in the child */

    stack_top = (uint64_t*)child_frame;

    linked_pcb* child = new_pcb(
        child_pml4_phys,
        parent->name,
        (uint64_t*)&stack_top,
        (stack_t){
            .bottom = (uintptr_t)k_stack,
            .top    = (uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES,
            .size   = DEFAULT_STACK_PAGE_BYTES
        },
        parent->user_stack,      /* same VA range; backed by freshly copied pages */
        PCB_STATE_STARTING
    );

    if (!child) {
        page_kfree((uintptr_t)k_stack, DEFAULT_STACK_PAGE_AMOUNT + 1);
        vmm_pagetable_free_owned(child_pml4_phys);
        return -E_NOMEM;
    }

    /* copied physically above; VAs and sizes stay identical to parent's */
    child->heap              = parent->heap;
    child->opened_file_table = parent->opened_file_table;
    child->cwd_i             = parent->cwd_i;

    child->state = PCB_STATE_RUNNING;


#if FORK_DEBUG
    Sys_Success("fork: pid %u -> child pid %u\n", parent->pid, child->pid);
#endif

    return child->pid;
}