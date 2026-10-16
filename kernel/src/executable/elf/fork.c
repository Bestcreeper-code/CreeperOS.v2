#include "fork.h"

#include "arch/locks.h"
#include "arch/vmm.h"
#include "config.h"
#include "debug/Logger.h"
#include "defines/err_codes.h"
#include "defines/lists.h"
#include "memops.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include "mm/vmm_arch.h"
#include "scheduler/scheduler.h"
#include "string/string.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
//todo: put actually decent code insetad of (atleast reviewed) ai stuff i cooked when tired 

static inline uint64_t* _table_virt(uint64_t entry)
{
    return (uint64_t*)PHYS_2_HHDM(entry & PTE_ADDR_MASK);
}

// same walk as execve.c's helper, duplicated here since that one is
// file-static -- returns the phys page backing 'va' in pml4, if present.
static physptr_t _walk_leaf_phys(uint64_t* pml4, uintptr_t va, bool* out_owned)
{
    uint32_t i4 = (va >> 39) & 0x1FF;
    uint32_t i3 = (va >> 30) & 0x1FF;
    uint32_t i2 = (va >> 21) & 0x1FF;
    uint32_t i1 = (va >> 12) & 0x1FF;

    if (!(pml4[i4] & PTE_PRESENT)) return 0;
    uint64_t* pdpt = _table_virt(pml4[i4]);

    if (!(pdpt[i3] & PTE_PRESENT)) return 0;
    uint64_t* pd = _table_virt(pdpt[i3]);

    if (!(pd[i2] & PTE_PRESENT)) return 0;
    uint64_t* pt = _table_virt(pd[i2]);

    uint64_t pte = pt[i1];
    if (!(pte & PTE_PRESENT)) return 0;

    if (out_owned) *out_owned = (pte & PTE_LOCAL_OWNED) != 0;
    return pte & PTE_ADDR_MASK;
}

static int _fork_copy_user_pages(uint64_t* old_pml4, uint64_t* new_pml4,
                                  uintptr_t skip_lo, uintptr_t skip_hi)
{
    for (uint32_t i4 = 0; i4 < 256; i4++) {
        if (!(old_pml4[i4] & PTE_PRESENT)) continue;
        uint64_t* pdpt = _table_virt(old_pml4[i4]);

        for (uint32_t i3 = 0; i3 < 512; i3++) {
            if (!(pdpt[i3] & PTE_PRESENT)) continue;
            uint64_t* pd = _table_virt(pdpt[i3]);

            for (uint32_t i2 = 0; i2 < 512; i2++) {
                if (!(pd[i2] & PTE_PRESENT)) continue;
                uint64_t* pt = _table_virt(pd[i2]);

                for (uint32_t i1 = 0; i1 < 512; i1++) {
                    uint64_t pte = pt[i1];
                    if (!(pte & PTE_PRESENT)) continue;

                    uintptr_t va = ((uintptr_t)i4 << 39) |
                                   ((uintptr_t)i3 << 30) |
                                   ((uintptr_t)i2 << 21) |
                                   ((uintptr_t)i1 << 12);

                    if (va >= skip_lo && va < skip_hi) continue;

                    physptr_t old_pa = pte & PTE_ADDR_MASK;
                    uint64_t  flags  = pte & ~PTE_ADDR_MASK;

                    if (pte & PTE_LOCAL_OWNED) {
                        physptr_t new_pa = pmm_alloc_pages(1);
                        if (!new_pa) return -E_NOMEM;

                        memcpy(PHYS_2_HHDM(new_pa), PHYS_2_HHDM(old_pa), PAGE_SIZE_4K);
                        map_4k(new_pml4, va, new_pa, flags);
                    } else {
                        map_4k(new_pml4, va, old_pa, flags);
                    }
                }
            }
        }
    }
    return 0;
}



static physptr_t _fork_copy_contig_block(uint64_t* new_pml4, uintptr_t va_base,
                                          physptr_t old_phys_base, size_t page_count,
                                          uint64_t flags)
{
    physptr_t new_phys = pmm_alloc_pages(page_count);
    if (!new_phys) return 0;

    memcpy(PHYS_2_HHDM(new_phys), PHYS_2_HHDM(old_phys_base), page_count * PAGE_SIZE_4K);
    map_4k_pages(new_pml4, va_base, new_phys, page_count, flags);
    return new_phys;
}



static void _fork_cleanup_proc(pcb_t* child, physptr_t new_pml4_phys)
{
    if (new_pml4_phys)
        vmm_pagetable_free_owned(new_pml4_phys); // frees every LOCAL_OWNED page we mapped

    if (child) {
        _free_tid(child->pid);
        if (child->name) kfree(child->name);
        kfree(child);
    }
}



pid_t fork_process(tcb_t* parent_thread, uint64_t rsp)
{
    if (!parent_thread || !parent_thread->process) return -E_INVAL;
    if (!rsp) return -E_INVAL;

    pcb_t* parent = parent_thread->process;
    if (parent == kernel_proc) {
        Sys_Error("fork: refusing to fork the kernel process\n");
        return -E_INVAL;
    }

    uint64_t* old_pml4 = PHYS_2_HHDM(parent->cr3);

    // ---- child pcb ----

    pcb_t* child = (pcb_t*)struct_kmalloc_align(pcb_t);
    if (!child) {
        Sys_Error("fork: unable to allocate child pcb\n");
        return -E_NOMEM;
    }
    memset(child, 0, sizeof(pcb_t));

    tid_t cpid = _get_unused_tid();
    if (cpid < 0) {
        Sys_Error("fork: no free pid available\n");
        kfree(child);
        return -E_NOMEM;
    }
    child->pid = cpid;

    child->name = strdup(parent->name);
    child->uid  = parent->uid;
    child->gid  = parent->gid;
    child->parent = parent;
    child->threads.first = NULL;
    child->childs.first  = NULL;
    child->signals_bitmap = parent->signals_bitmap;
    memcpy(child->signal_handlers_array, parent->signal_handlers_array,
           sizeof(child->signal_handlers_array));

    // ---- address space ----

    physptr_t new_pml4_phys = pmm_alloc_pages_zeroed(1);
    if (!new_pml4_phys) {
        Sys_Error("fork: unable to allocate pml4\n");
        _fork_cleanup_proc(child, 0);
        return -E_NOMEM;
    }
    uint64_t* new_pml4 = PHYS_2_HHDM(new_pml4_phys);

    for (int i = 256; i < 512; i++)
        new_pml4[i] = old_pml4[i]; // kernel half is shared, never LOCAL_OWNED

    // heap
    if (parent->heap.lower) {
        bool owned;
        physptr_t heap_old_phys = _walk_leaf_phys(old_pml4, parent->heap.lower, &owned);
        if (!heap_old_phys) {
            Sys_Error("fork: couldn't locate parent heap phys\n");
            _fork_cleanup_proc(child, new_pml4_phys);
            return -E_NOMEM;
        }

        size_t heap_pages = parent->heap.size / PAGE_SIZE_4K;
        physptr_t new_heap = _fork_copy_contig_block(
            new_pml4, parent->heap.lower, heap_old_phys, heap_pages,
            PTE_USER | PTE_WRITABLE | PTE_LOCAL_OWNED);
        if (!new_heap) {
            Sys_Error("fork: unable to copy heap\n");
            _fork_cleanup_proc(child, new_pml4_phys);
            return -E_NOMEM;
        }

        child->heap.lower  = parent->heap.lower;
        child->heap.higher = parent->heap.higher;
        child->heap.size   = parent->heap.size;
    }

    // opened file table -- raw copy: fds/offsets are duplicated as-is,
    // there's no refcounting on the underlying files in this codebase yet
    if (parent->opened_file_table) {
        bool owned;
        physptr_t ft_old_phys = _walk_leaf_phys(old_pml4, parent->opened_file_table, &owned);
        if (!ft_old_phys) {
            Sys_Error("fork: couldn't locate parent file table phys\n");
            _fork_cleanup_proc(child, new_pml4_phys);
            return -E_NOMEM;
        }

        physptr_t new_ft = _fork_copy_contig_block(
            new_pml4, parent->opened_file_table, ft_old_phys,
            DEFAULT_FILE_TABLE_PAGES, PTE_WRITABLE | PTE_LOCAL_OWNED);
        if (!new_ft) {
            Sys_Error("fork: unable to copy file table\n");
            _fork_cleanup_proc(child, new_pml4_phys);
            return -E_NOMEM;
        }

        child->opened_file_table = parent->opened_file_table;
    }

    child->cwd_i = parent->cwd_i;

    // main thread's user stack -- fixed at the top of the stack region,
    // same convention as new_usproc(). NOTE: only correct for forking
    // the process's main thread; secondary threads' stacks have no
    // stored VA anywhere in tcb_t, so they can't be relocated here.
    uintptr_t stack_va_base = STACK_UPPER_USPACE_ADDR - DEFAULT_STACK_PAGE_BYTES;
    physptr_t new_stack_phys;
    {
        bool owned;
        physptr_t stack_old_phys = _walk_leaf_phys(old_pml4, stack_va_base, &owned);
        if (!stack_old_phys) {
            Sys_Error("fork: couldn't locate parent user stack phys\n");
            _fork_cleanup_proc(child, new_pml4_phys);
            return -E_NOMEM;
        }

        new_stack_phys = _fork_copy_contig_block(
            new_pml4, stack_va_base, stack_old_phys, DEFAULT_STACK_PAGE_AMOUNT,
            PTE_WRITABLE | PTE_NX | PTE_USER | PTE_LOCAL_OWNED);
        if (!new_stack_phys) {
            Sys_Error("fork: unable to copy user stack\n");
            _fork_cleanup_proc(child, new_pml4_phys);
            return -E_NOMEM;
        }
    }

    // everything else (elf image, any future mmap-style regions):
    // generic per-page copy, skipping the fixed regions above
    if (_fork_copy_user_pages(old_pml4, new_pml4,
            DEFAULT_USER_HEAP_START, DEFAULT_USER_HEAP_END) < 0 ||
        _fork_copy_user_pages(old_pml4, new_pml4,
            DEFAULT_FILE_TABLE_START,
            DEFAULT_FILE_TABLE_START + DEFAULT_FILE_TABLE_PAGES * PAGE_SIZE_4K) < 0 ||
        _fork_copy_user_pages(old_pml4, new_pml4,
            stack_va_base, stack_va_base + DEFAULT_STACK_PAGE_BYTES) < 0)
    {
        Sys_Error("fork: unable to copy remaining user pages\n");
        _fork_cleanup_proc(child, new_pml4_phys);
        return -E_NOMEM;
    }

    child->cr3 = new_pml4_phys;

    // ---- child thread ----

    tcb_t* child_thread = (tcb_t*)struct_kmalloc_align(tcb_t);
    if (!child_thread) {
        Sys_Error("fork: unable to allocate child thread\n");
        _fork_cleanup_proc(child, new_pml4_phys);
        return -E_NOMEM;
    }
    memset(child_thread, 0, sizeof(tcb_t));
    memcpy(child_thread->fp_state, parent_thread->fp_state, sizeof(child_thread->fp_state));
    child_thread->process     = child;
    child_thread->fs_base     = parent_thread->fs_base;
    child_thread->gs_base     = parent_thread->gs_base;
    child_thread->signal_mask = parent_thread->signal_mask;

    void* k_stack = (void*)page_kalloc(DEFAULT_STACK_PAGE_AMOUNT + 1, PTE_WRITABLE);
    if (!k_stack) {
        Sys_Error("fork: unable to allocate kernel stack\n");
        kfree(child_thread);
        _fork_cleanup_proc(child, new_pml4_phys);
        return -E_NOMEM;
    }
    child_thread->kernel_stack.top    = (uintptr_t)k_stack;
    child_thread->kernel_stack.bottom = (uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES;
    child_thread->kernel_stack.size   = DEFAULT_STACK_PAGE_BYTES;

    child_thread->user_stack.top    = new_stack_phys;
    child_thread->user_stack.bottom = new_stack_phys + DEFAULT_STACK_PAGE_BYTES;
    child_thread->user_stack.size   = DEFAULT_STACK_PAGE_BYTES;

    child_thread->tid = _get_unused_tid();
    if (child_thread->tid < 0) {
        Sys_Error("fork: no free tid available for child thread\n");
        page_kfree((uintptr_t)k_stack, DEFAULT_STACK_PAGE_AMOUNT + 1);
        kfree(child_thread);
        _fork_cleanup_proc(child, new_pml4_phys);
        return -E_NOMEM;
    }

    // mirror the parent's in-flight kernel stack from the current frame
    // (rsp) up to the top, so the child resumes at the exact same rip
    // as the parent -- just with a 0 return value, per fork() semantics.
    if (rsp < parent_thread->kernel_stack.top ||
        rsp >= parent_thread->kernel_stack.bottom) {
        Sys_Error("fork: rsp not within parent's kernel stack\n");
        _free_tid(child_thread->tid);
        page_kfree((uintptr_t)k_stack, DEFAULT_STACK_PAGE_AMOUNT + 1);
        kfree(child_thread);
        _fork_cleanup_proc(child, new_pml4_phys);
        return -E_INVAL;
    }

    size_t used = parent_thread->kernel_stack.bottom - rsp;
    uintptr_t child_rsp = child_thread->kernel_stack.bottom - used;

    memcpy((void*)child_rsp, (void*)rsp, used);

    process_stack_frame* child_frame = (process_stack_frame*)child_rsp;
    child_frame->rax = 0; // fork() returns 0 in the child

    child_thread->k_rsp = child_rsp;
    child_thread->state = TCB_STATE_RUNNABLE;

    // ---- link in ----

    unsigned long irq_flags = spin_lock_irqsave(&scheduler_lock);

    hlist_add_head(&child_thread->proc_list_node, &child->threads);
    hlist_add_head(&child_thread->shed_list_node, &scheduler_threads);
    hlist_add_head(&child->proclist_node, &_scheduler_processes_list_head);
    hlist_add_head(&child->siblings_list_node, &parent->childs);

    id_table_set_proc(child->pid, child);
    id_table_set_thread(child_thread->tid, child_thread);

    process_list_depth++;
    spin_unlock_irqrestore(&scheduler_lock, irq_flags);

    Sys_Success("fork: pid %u -> child pid %u (tid %u)\n",
                (unsigned)parent->pid, (unsigned)child->pid, (unsigned)child_thread->tid);

    return child->pid;
}