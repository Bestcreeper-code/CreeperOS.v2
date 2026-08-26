#include "arch/locks.h"
#include "arch/vmm.h"
#include "arch/x86_64/cpu/gdt.h"
#include "asm/asm.h"
#include "config.h"
#include "debug/Logger.h"
#include "debug/panic.h"
#include "defines/container_of.h"
#include "defines/err_codes.h"
#include "defines/lists.h"
#include "drivers/pit/pit.h"
#include "memops.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include "mm/vmm_arch.h"
#include "scheduler.h"
#include "printf/printf.h"
#include "string/string.h"
#include "vfs/vfs.h"

#include "defines/auxv.h"
#include <stddef.h>
#include <stdint.h>




HLIST_HEAD(_scheduler_processes_list_head);

uint16_t process_list_depth = 1;

pcb_t* current_process;
tcb_t* current_thread;

pcb_t* kernel_proc;

HLIST_HEAD(scheduler_threads);

#define _TID_BITMAP_SIZE (MAX_TID / 64)

uint8_t task_switching_flag = 0;

uint64_t tid_bitmap[_TID_BITMAP_SIZE] = {[0 ... _TID_BITMAP_SIZE-1] = 0xFFFFFFFFULL};
int tid_bitmap_curr = 0;

size_t scheduler_lock = 0;


bool tid0_used = false;

tid_t _get_unused_tid() {
    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    tid_t result = (tid_t)-1;

    for (int i = tid_bitmap_curr & ~(0x3F); i < _TID_BITMAP_SIZE; i++) {
        uint64_t* current = &tid_bitmap[i];


        if (!*current) continue;

        for (int j = tid_bitmap_curr & 0x3F; j < 64; j++) {
            
            if (tid0_used && i == 0 && j == 0) continue;

            if (*current & (1ULL << j)) {
                *current &= ~(1ULL << j);
                result = (tid_t)(i * 64 + j);
                goto done;
            }
        }
    }

done:
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);
    return result;
}

void _free_tid(tid_t tid) {
    uint16_t index = tid / 64;
    uint8_t bit = tid % 64;

    if (index >= _TID_BITMAP_SIZE) return;

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    tid_bitmap[index] |= (1ULL << bit);
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);
}

void* id_table[MAX_TID]; //NULL = unused

void* id_lookup(uint16_t id, id_kind_t* kind_out) {
    if (id >= MAX_TID) { if (kind_out) *kind_out = ID_NONE; return NULL; }
    uintptr_t raw = (uintptr_t)id_table[id];
    if (!raw) { if (kind_out) *kind_out = ID_NONE; return NULL; }
    if (kind_out) *kind_out = (raw & ID_TAG_MASK) ? ID_THREAD : ID_PROC;
    return (void*)(raw | ID_TAG_MASK); //set last bit since anyway will always be 1 in kernel address space
}






#define SCHED_LOG_INTERVAL 2000U

void _log_all_processes(void) {
    Sys_log("process list\n");
    Sys_log("-----------------------------------------------------------------------------------------------\n");

    struct hlist_node* pn = _scheduler_processes_list_head.first;
    while (pn) {
        pcb_t* p = container_of(pn, pcb_t, proclist_node);

        Sys_Info(
            "pid=%u name=%s cr3=0x%016llx heap=[0x%016llx-0x%016llx] "
            "files=%p cwd=%p\n",
            (unsigned)p->pid,
            p->name ? p->name : "(null)",
            (unsigned long long)p->cr3,
            (unsigned long long)p->heap.lower,
            (unsigned long long)p->heap.higher,
            (void*)p->opened_file_table,
            (void*)p->cwd_i
        );

        struct hlist_node* tn = p->threads.first;
        while (tn) {
            tcb_t* t = container_of(tn, tcb_t, proc_list_node);
            Sys_Info(
                "  tid=%u state=0x%04x k_rsp=0x%016llx "
                "kstack=[0x%016llx-0x%016llx] ustack=[0x%016llx-0x%016llx] "
                "exit=%d\n",
                (unsigned)t->tid,
                (unsigned)t->state,
                (unsigned long long)t->k_rsp,
                (unsigned long long)t->kernel_stack.bottom,
                (unsigned long long)t->kernel_stack.top,
                (unsigned long long)t->user_stack.bottom,
                (unsigned long long)t->user_stack.top,
                t->exit_code
            );
            tn = tn->next;
        }

        pn = pn->next;
    }

    Sys_log("-----------------------------------------------------------------------------------------------\n\n");
}

const char* _tcb_state_str(uint16_t state) {
    if (state & TCB_STATE_DEAD)     return "DEAD";
    if (state & TCB_STATE_ZOMBIE)        return "ZOMBIE";
    if (state & TCB_STATE_STARTING_UP) return "STARTING UP";
    if (state & TCB_STATE_RUNNABLE)     return "RUNNABLE";
    return "UNKNOWN";
}

void _log_process_tree() {
    Sys_log("process tree\n");
    Sys_log("-----------------------------------------------------------------------------------------------\n");

    struct hlist_node* pn = _scheduler_processes_list_head.first;
    while (pn) {
        pcb_t* p = container_of(pn, pcb_t, proclist_node);

        Sys_log("%s (pid=%u)\n", p->name ? p->name : "(null)", (unsigned)p->pid);

        struct hlist_node* tn = p->threads.first;
        while (tn) {
            tcb_t* t = container_of(tn, tcb_t, proc_list_node);
            bool is_last = (tn->next == NULL);

            Sys_log("  %s tid=%u [%s]\n",
                    is_last ? "\\-" : "|-",
                    (unsigned)t->tid,
                    _tcb_state_str(t->state));

            tn = tn->next;
        }

        pn = pn->next;
    }

    Sys_log("-----------------------------------------------------------------------------------------------\n\n");
}


static void _unlink_thread(tcb_t* t) {
    hlist_del(&t->shed_list_node);
    hlist_del(&t->proc_list_node);
}

static void _teardown_process(pcb_t* proc) {
    hlist_del(&proc->proclist_node);
    process_list_depth--;

    if (proc->heap.lower)
        pmm_free_pages(ADDR_TO_PAGE(proc->heap.lower),
                        ADDR_TO_PAGE(proc->heap.size));

    if (proc->opened_file_table)
        pmm_free_pages(ADDR_TO_PAGE(proc->opened_file_table),
                        DEFAULT_FILE_TABLE_PAGES);

    vmm_pagetable_free_owned(proc->cr3);
    id_table_clear(proc->pid);
    kfree(proc->name);
    kfree(proc);
}


int kill_ktask(tcb_t* t) {
    if (!t) return -1;
    if (t->process != kernel_proc) {
        Sys_Error("kill_ktask: tid %u is not a kernel thread\n", t->tid);
        return -1;
    }

    Sys_log("ktask %u exited with code:%d\n", t->tid, t->exit_code);

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    _unlink_thread(t);
    id_table_clear(t->tid);
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    _free_tid(t->tid);

    page_kfree(t->kernel_stack.top, ADDR_TO_PAGE(t->kernel_stack.size));

    kfree(t);
    return 0;
}


static void reaper_ktask() {
    while (true) {
        tcb_t* victim = NULL;

        acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
        struct hlist_node* n = scheduler_threads.first;
        while (n) {
            tcb_t* t = container_of(n, tcb_t, shed_list_node);
            if (t->state & TCB_STATE_DEAD) { victim = t; break; }
            n = n->next;
        }
        release_lock(&scheduler_lock, SCHED_LOCK_BIT);

        if (victim) { kill_ktask(victim); continue; }

        pcb_t* orphan = NULL;
        acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
        struct hlist_node* pn = _scheduler_processes_list_head.first;
        while (pn) {
            pcb_t* p = container_of(pn, pcb_t, proclist_node);
            if ((p->state & PCB_STATE_ZOMBIE) && !p->parent) { orphan = p; break; }
            pn = pn->next;
        }
        release_lock(&scheduler_lock, SCHED_LOCK_BIT);

        if (orphan) { reap_us_proc_threads(orphan); continue; }

        _yield();
    }
}


int scheduler_init() {
    Sys_Info("Initing PIT\n");
    pit_init();
    Sys_Info("Initing Scheduler\n");

    // handmade kernel pcb
    kernel_proc = (pcb_t*)struct_kmalloc_align(pcb_t);
    if (!kernel_proc) {
        Sys_Error("scheduler_init: unable to allocate kernel_proc\n");
        return -1;
    }
    memset(kernel_proc, 0, sizeof(pcb_t));

    kernel_proc->pid  = 0;
    kernel_proc->name = strdup("Kernel");
    kernel_proc->cr3  = kernel_pagedir_phys;
    kernel_proc->uid  = 0;
    kernel_proc->gid  = 0;
    kernel_proc->threads.first = NULL;

    // handmade boot tcb
    tcb_t* boot_thread = (tcb_t*)struct_kmalloc_align(tcb_t);
    if (!boot_thread) {
        Sys_Error("scheduler_init: unable to allocate boot thread\n");
        kfree(kernel_proc->name);
        kfree(kernel_proc);
        return -1;
    }
    memset(boot_thread, 0, sizeof(tcb_t));

    *(uint32_t*)(boot_thread->fp_state + 24) = 0x1F80; // MXCSR 
    boot_thread->process = kernel_proc;
    boot_thread->tid     = 0; // reserved boot tid
    boot_thread->state   = TCB_STATE_RUNNABLE;

    tid0_used = true;

    hlist_add_head(&boot_thread->proc_list_node, &kernel_proc->threads);
    hlist_add_head(&boot_thread->shed_list_node, &scheduler_threads);
    hlist_add_head(&kernel_proc->proclist_node, &_scheduler_processes_list_head);
    id_table_set_proc(kernel_proc->pid, kernel_proc);
    process_list_depth = 1;

    current_thread  = boot_thread;
    current_process = kernel_proc;

    enable_scheduler();
// #fix teh damn reaper killing himself since ktask are shit now smh
    Sys_Debug("reaper tid = %04d", ktask_start(reaper_ktask, "reaper")->tid);

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

tcb_t* ktask_start(void* entry, char* name) {
    tcb_t* t = new_thread(NULL, entry, TCB_STATE_RUNNABLE);
    if (!t) {
        Sys_Error("ktask_start: unable to create thread '%s'\n", name);
        return NULL;
    }

    Sys_Debug("ktask_start: created ktask '%s' tid=%u\n", name, t->tid);
    return t;
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
    size_t auxv_bytes = 48 * sizeof(uint64_t); //margin to be safe

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

    frame->rip = entry;
    frame->cs = cs;
    frame->rflags = 0x202;
    frame->rsp = user_rsp;
    frame->ss = ss;

    *stack_top = (uint64_t*)frame;
}

pcb_t* new_usproc(pcb_t* parent, void* entry, char* name, physptr_t page_dir,
                   char** argv, char** envp) {
    Sys_Debug("Starting user task '%s'(cr3: %p)\n", name, (void*)page_dir);
    if (!page_dir) {
        Sys_Error("task '%s': no page directory provided\n", name);
        return NULL;
    }

    uint64_t* p_dir = PHYS_2_HHDM(page_dir);

    // kernel space
    for (int i = 256; i < 512; i++)
        p_dir[i] = ((uint64_t*)PHYS_2_HHDM(kernel_pagedir_phys))[i];

    //pcb init

    pcb_t* proc = (pcb_t*)struct_kmalloc_align(pcb_t);
    if (!proc) {
        Sys_Error("task '%s': unable to allocate pcb\n", name);
        return NULL;
    }
    memset(proc, 0, sizeof(pcb_t));

    tid_t pid = _get_unused_tid();
    if (pid < 0) {
        Sys_Error("task '%s': no free pid available\n", name);
        kfree(proc);
        return NULL;
    }

    proc->pid  = pid;
    proc->name = strdup(name);
    proc->cr3  = page_dir;
    proc->uid  = 0;
    proc->gid  = 0;
    proc->threads.first = NULL;
    proc->parent = parent;          // NULL is valid: e.g. init/root has no parent
    proc->childs.first = NULL;
    

    physptr_t heap_pages = pmm_alloc_pages(DEFAULT_USER_HEAP_PAGES);
    if (!heap_pages) {
        Sys_Error("task '%s': unable to allocate user heap pages\n", name);
        _free_tid(proc->pid);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    map_4k_pages(
        p_dir,
        DEFAULT_USER_HEAP_START, heap_pages,
        DEFAULT_USER_HEAP_PAGES, PTE_USER | PTE_WRITABLE | PTE_LOCAL_OWNED
    );

    proc->heap.lower  = DEFAULT_USER_HEAP_START;
    proc->heap.higher = DEFAULT_USER_HEAP_END;
    proc->heap.size   = DEFAULT_USER_HEAP_PAGES * PAGE_SIZE_4K;

    physptr_t ft_pages = pmm_alloc_pages(256 / (PAGE_SIZE_4K / sizeof(file)));
    if (!ft_pages) {
        Sys_Error("task '%s': unable to allocate user file table pages\n", name);
        pmm_free_pages(heap_pages, DEFAULT_USER_HEAP_PAGES);
        _free_tid(proc->pid);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    map_4k_pages(
        p_dir,
        DEFAULT_FILE_TABLE_START, ft_pages,
        DEFAULT_FILE_TABLE_PAGES, PTE_WRITABLE | PTE_LOCAL_OWNED
    );

    proc->opened_file_table = DEFAULT_FILE_TABLE_START;
    proc->cwd_i = NULL;

    //main/initial thread 

    tcb_t* main_thread = (tcb_t*)struct_kmalloc_align(tcb_t);
    if (!main_thread) {
        Sys_Error("task '%s': unable to allocate main thread\n", name);
        pmm_free_pages(ft_pages, DEFAULT_FILE_TABLE_PAGES);
        pmm_free_pages(heap_pages, DEFAULT_USER_HEAP_PAGES);
        _free_tid(proc->pid);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }
    memset(main_thread, 0, sizeof(tcb_t));
    *(uint32_t*)(main_thread->fp_state + 24) = 0x1F80; // MXCSR def
    main_thread->process = proc;

    void* k_stack = (void*)page_kalloc(DEFAULT_STACK_PAGE_AMOUNT + 1, PTE_WRITABLE);
    if (!k_stack) {
        Sys_Error("task '%s': unable to allocate kernel stack\n", name);
        kfree(main_thread);
        pmm_free_pages(ft_pages, DEFAULT_FILE_TABLE_PAGES);
        pmm_free_pages(heap_pages, DEFAULT_USER_HEAP_PAGES);
        _free_tid(proc->pid);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    main_thread->kernel_stack.top    = (uintptr_t)k_stack;
    main_thread->kernel_stack.bottom = (uintptr_t)k_stack + DEFAULT_STACK_PAGE_BYTES;
    main_thread->kernel_stack.size   = DEFAULT_STACK_PAGE_BYTES;

    page_index us_stack_pages = pmm_alloc_pages(DEFAULT_STACK_PAGE_AMOUNT);
    if (!us_stack_pages) {
        Sys_Error("task '%s': unable to allocate user stack pages\n", name);
        page_kfree((uintptr_t)k_stack, DEFAULT_STACK_PAGE_AMOUNT + 1);
        kfree(main_thread);
        pmm_free_pages(ft_pages, DEFAULT_FILE_TABLE_PAGES);
        pmm_free_pages(heap_pages, DEFAULT_USER_HEAP_PAGES);
        _free_tid(proc->pid);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    uintptr_t us_stack_bott = us_stack_pages;

    for (int i = 0; i < DEFAULT_STACK_PAGE_AMOUNT; i++) {
        uintptr_t page_addr = us_stack_bott + i * PAGE_SIZE_4K;
        map_4k(p_dir, STACK_UPPER_USPACE_ADDR - (DEFAULT_STACK_PAGE_BYTES - i * PAGE_SIZE_4K),
            page_addr, PTE_WRITABLE | PTE_NX | PTE_USER | PTE_LOCAL_OWNED);
    }

    main_thread->user_stack.top    = us_stack_bott;
    main_thread->user_stack.bottom = us_stack_bott + DEFAULT_STACK_PAGE_BYTES;
    main_thread->user_stack.size   = DEFAULT_STACK_PAGE_BYTES;

    void* us_stack_khost_top = (uint8_t*)PHYS_2_HHDM(us_stack_bott) + DEFAULT_STACK_PAGE_BYTES;

    uint64_t user_rsp;
    if (!_setup_user_stack_args(us_stack_khost_top, STACK_UPPER_USPACE_ADDR,
                                 argv, envp, &user_rsp)) {
        Sys_Error("task '%s': unable to set up argv/envp on user stack\n", name);
        pmm_free_pages(us_stack_bott, DEFAULT_STACK_PAGE_AMOUNT);
        page_kfree((uintptr_t)k_stack, DEFAULT_STACK_PAGE_AMOUNT + 1);
        kfree(main_thread);
        pmm_free_pages(ft_pages, DEFAULT_FILE_TABLE_PAGES);
        pmm_free_pages(heap_pages, DEFAULT_USER_HEAP_PAGES);
        _free_tid(proc->pid);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    main_thread->tid = _get_unused_tid();
    if (main_thread->tid < 0) {
        Sys_Error("task '%s': no free tid available for main thread\n", name);
        pmm_free_pages(us_stack_bott, DEFAULT_STACK_PAGE_AMOUNT);
        page_kfree((uintptr_t)k_stack, DEFAULT_STACK_PAGE_AMOUNT + 1);
        kfree(main_thread);
        pmm_free_pages(ft_pages, DEFAULT_FILE_TABLE_PAGES);
        pmm_free_pages(heap_pages, DEFAULT_USER_HEAP_PAGES);
        _free_tid(proc->pid);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    main_thread->state = TCB_STATE_STARTING_UP;
    main_thread->k_rsp = main_thread->kernel_stack.bottom;

    _build_user_stack_frame(
        (uint64_t**)&main_thread->k_rsp,
        (uint64_t)entry,
        user_rsp,
        USER_CODE_SEGMENT, USER_DATA_SEGMENT
    );

    main_thread->fs_base = 0;
    main_thread->gs_base = 0;

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);

    hlist_add_head(&main_thread->proc_list_node, &proc->threads);
    hlist_add_head(&main_thread->shed_list_node, &scheduler_threads);
    hlist_add_head(&proc->proclist_node, &_scheduler_processes_list_head);
    if (parent)
        hlist_add_head(&proc->siblings_list_node, &parent->childs);
    
    id_table_set_proc(proc->pid, proc);
    id_table_set_thread(main_thread->tid, main_thread);

    process_list_depth++;
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    Sys_Success("task '%s': created sucesssfully\n", name);
    return proc;
}


//exits
static void _usthread_exit_locked(tcb_t* t, int exit_code) {
    t->exit_code = exit_code;
    hlist_del(&t->shed_list_node);
    t->state = TCB_STATE_ZOMBIE;
}

void usthread_exit(tcb_t* t, int exit_code) {
    if (!t || t->process == kernel_proc) return;

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    if (t->state & TCB_STATE_ZOMBIE) { release_lock(&scheduler_lock, SCHED_LOCK_BIT); return; }
    _usthread_exit_locked(t, exit_code);
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    if (t->user_stack.top) {
        pmm_free_pages(ADDR_TO_PAGE(t->user_stack.top), ADDR_TO_PAGE(t->user_stack.size));
        t->user_stack = (stack_t){0};
    }
    
}

void usproc_exit(pcb_t* proc, int exit_code) {
    if (!proc || proc == kernel_proc) return;

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    if (proc->state & PCB_STATE_ZOMBIE) { release_lock(&scheduler_lock, SCHED_LOCK_BIT); return; }

    struct hlist_node* tn = proc->threads.first;
    while (tn) {
        tcb_t* t = container_of(tn, tcb_t, proc_list_node);
        if (!(t->state & TCB_STATE_ZOMBIE))
            _usthread_exit_locked(t, (t == current_thread) ? exit_code : -1);
        tn = tn->next;
    }
    proc->exit_code = exit_code;
    proc->state      = PCB_STATE_ZOMBIE;
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    
    tn = proc->threads.first;
    while (tn) {
        tcb_t* t = container_of(tn, tcb_t, proc_list_node);
        if (t->user_stack.top) {
            pmm_free_pages(ADDR_TO_PAGE(t->user_stack.top), ADDR_TO_PAGE(t->user_stack.size));
            t->user_stack = (stack_t){0};
        }
        tn = tn->next;
    }

    if (proc->heap.lower) {
        pmm_free_pages(ADDR_TO_PAGE(proc->heap.lower), ADDR_TO_PAGE(proc->heap.size));
        proc->heap.lower = 0;
    }
    if (proc->opened_file_table) {
        pmm_free_pages(ADDR_TO_PAGE(proc->opened_file_table), DEFAULT_FILE_TABLE_PAGES);
        proc->opened_file_table = 0;
    }
}

//reapers
int reap_us_thread(tcb_t* t) {
    if (!t) return -1;
    pcb_t* proc = t->process;
    if (!proc || proc == kernel_proc) {
        Sys_Error("reap_us_thread: tid %u has no user process\n", t->tid);
        return -1;
    }
    if (!(t->state & TCB_STATE_ZOMBIE)) {
        Sys_Error("reap_us_thread: tid %u has not exited\n", t->tid);
        return -1;
    }

    Sys_log("reaped thread %u (proc '%s') exit=%d\n", t->tid, proc->name, t->exit_code);

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    hlist_del(&t->proc_list_node);
    id_table_clear(t->tid);
    bool proc_empty = (proc->threads.first == NULL);
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    _free_tid(t->tid);
    page_kfree(t->kernel_stack.top, ADDR_TO_PAGE(t->kernel_stack.size));
    kfree(t);

    if (proc_empty && (proc->state & PCB_STATE_ZOMBIE))
        reap_us_proc(proc);

    return 0;
}

int reap_us_proc(pcb_t* proc) {
    if (!proc || proc == kernel_proc) return -1;
    if (!(proc->state & PCB_STATE_ZOMBIE)) {
        Sys_Error("reap_us_proc: proc '%s' has not exited\n", proc->name);
        return -1;
    }
    if (proc->threads.first) {
        Sys_Error("reap_us_proc: proc '%s' still has un-reaped threads\n", proc->name);
        return -1;
    }

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    hlist_del(&proc->proclist_node);
    if (proc->parent) hlist_del(&proc->siblings_list_node);
    process_list_depth--;
    id_table_clear(proc->pid);
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    vmm_pagetable_free_owned(proc->cr3);  // safe now: nothing of this proc runs anymore
    _free_tid(proc->pid);
    kfree(proc->name);
    kfree(proc);
    return 0;
}

void reap_us_proc_threads(pcb_t* proc) {
    struct hlist_node* tn;
    while ((tn = proc->threads.first))
        reap_us_thread(container_of(tn, tcb_t, proc_list_node));
}









    
    











tcb_t* new_thread(pcb_t* proc, void* entry, uint16_t starting_state) {
    bool is_kthread = (proc == NULL);
    if (is_kthread) proc = kernel_proc;

    tcb_t* new_thread = (tcb_t*)struct_kmalloc_align(tcb_t);
    if (!new_thread) return NULL;
    memset(new_thread, 0, sizeof(tcb_t));

    *(uint32_t*)(new_thread->fp_state + 24) = 0x1F80; // MXCSR def
    new_thread->process = proc;

    size_t kstack_amount = DEFAULT_STACK_PAGE_AMOUNT;
    physptr_t kstack_pages = page_kalloc(kstack_amount, PTE_WRITABLE);
    if (!kstack_pages) {
        kfree(new_thread);
        return NULL;
    }

    new_thread->kernel_stack.top    = kstack_pages;
    new_thread->kernel_stack.bottom = kstack_pages + DEFAULT_STACK_PAGE_BYTES;
    new_thread->kernel_stack.size   = DEFAULT_STACK_PAGE_BYTES;

    new_thread->tid = _get_unused_tid();
    if (new_thread->tid < 0) {
        page_kfree(kstack_pages, kstack_amount);
        kfree(new_thread);
        return NULL;
    }

    new_thread->state = starting_state;
    new_thread->k_rsp = new_thread->kernel_stack.bottom;

    if (is_kthread) {
        // no ustack, kernel stack frame(kernel segments)
        _build_kernel_stack_frame(&new_thread->k_rsp, (uint64_t)entry);
        new_thread->user_stack = (stack_t){0};
    } else {
        physptr_t ustack_pages = pmm_alloc_pages(DEFAULT_STACK_PAGE_AMOUNT);
        if (!ustack_pages) {
            _free_tid(new_thread->tid);
            page_kfree(kstack_pages, kstack_amount);
            kfree(new_thread);
            return NULL;
        }

        uintptr_t ustack_vbase = uvma_map_pages(
            (uint64_t*)PHYS_2_HHDM(proc->cr3),
            ustack_pages,
            DEFAULT_STACK_PAGE_AMOUNT,
            PTE_USER | PTE_WRITABLE | PTE_NX | PTE_LOCAL_OWNED,
            STACK_LOWER_USPACE_ADDR
        );
        if (!ustack_vbase) {
            pmm_free_pages(ustack_pages, DEFAULT_STACK_PAGE_AMOUNT);
            _free_tid(new_thread->tid);
            page_kfree(kstack_pages, kstack_amount);
            kfree(new_thread);
            return NULL;
        }

        new_thread->user_stack.top    = ustack_pages;
        new_thread->user_stack.bottom = ustack_pages + DEFAULT_STACK_PAGE_BYTES;
        new_thread->user_stack.size   = DEFAULT_STACK_PAGE_BYTES;

        _build_user_stack_frame(
            (uint64_t**)&new_thread->k_rsp,
            (uint64_t)entry,
            ustack_vbase,
            USER_CODE_SEGMENT,
            USER_DATA_SEGMENT
        );
    }

    acquire_lock(&scheduler_lock, SCHED_LOCK_BIT);
    hlist_add_head(&new_thread->proc_list_node, &proc->threads);
    hlist_add_head(&new_thread->shed_list_node, &scheduler_threads);
    id_table_set_thread(new_thread->tid, new_thread);
    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    return new_thread;
}



















extern uint8_t fpu_enabled;
void* sched_next_process_core(uint64_t saved_rsp) {
    if(!try_acquire_lock(&scheduler_lock, SCHED_LOCK_BIT)) {
        return (void*)saved_rsp;//later would be other probably 
    }
    tcb_t* current = current_thread;
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
    
    // static char buffer[256];
    // sprintf_(buffer,"%d\n",current->tid);
    // serial_write_string(buffer);
    struct hlist_node* next_node = current->shed_list_node.next;
    tcb_t* next = NULL;

    while (true) {
        if (!next_node)
            next_node = scheduler_threads.first;

        if (!next_node) {
            next = current;
            break;
        }

        tcb_t* cand = container_of(next_node, tcb_t, shed_list_node);

        if (cand->state & TCB_STATE_RUNNABLE) {
            next = cand;
            break;
        }

        if (next_node == &current->shed_list_node) {
            next = current;
            break;
        }

        next_node = next_node->next;
    }

    current_thread  = next;
    current_process = next->process;

    release_lock(&scheduler_lock, SCHED_LOCK_BIT);

    __asm__ volatile ("mov %0, %%cr3" :: "r"(next->process->cr3) : "memory");

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