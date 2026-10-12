#include "execve.h"
#include "asm/asm.h"
#include "defines/container_of.h"
#include "defines/lists.h"
#include "executable/elf/elf.h"
#include "fork.h" // for PTE_ADDR_MASK / PT_ENTRIES / _table_virt-style helpers if shared there

#include "config.h"
#include "scheduler/scheduler.h"
#include "mm/vmm_arch.h"
#include "memory/pmm.h"
#include "memory/memory.h"
#include "defines/err_codes.h"
#include "vfs/vfs.h"
#include "vfs/fs.h"
#include "debug/Logger.h"
#include "string/string.h"
#include "memops.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


int exec_elf_from_vfs(tcb_t* thread, const char* vfs_path,
                       char** argv, char** envp, uint64_t rsp)
{
    if (!thread || !thread->process)
        return -E_INVAL;

    pcb_t* proc = thread->process;

    if (!proc->cr3 || proc->cr3 == kernel_pagedir_phys)
        return -E_INVAL;

    if (!rsp)
        return -E_INVAL;

    process_stack_frame* frame = (process_stack_frame*)rsp;

    dentry* dent = kpath_lookup(root_dentry->inode, vfs_path);
    if(!dent)return -E_NOENT;
    cli();
    pid_t newpid = load_elf_from_vfs(vfs_path, argv, envp);
    if(newpid<0) return newpid;
    
    id_kind_t kind_out;
    pcb_t* newproc = id_lookup(newpid, &kind_out);
    if (!newproc || kind_out != ID_PROC ) {
        Sys_Error("execve load fail");
        sti();
        return -E_NOMEM;
    }

    
    id_table_clear(newproc->pid);
    newproc->pid = proc->pid;
    id_table_set_proc(proc->pid, newproc);
    
    if(newproc->threads.first == NULL) {
        sti();
        return -E_NOMEM;
    }
    
    tcb_t* mainthread = container_of(newproc->threads.first, tcb_t, proc_list_node);
    
    // fd table
    file* src = (file*)proc->opened_file_table;
    file* dst = (file*)newproc->opened_file_table;

    for (size_t i = 0; i < 256; i++) {
        if (!src[i].f_inode){
            continue;
        }
        dst[i] = src[i];
        memset(&src[i], 0, sizeof(src[i]));
    }
    
    // cwd
    newproc->cwd_i = proc->cwd_i;
    proc->cwd_i = NULL;

    // creds
    newproc->uid = proc->uid;
    newproc->gid = proc->gid;

    id_table_clear(mainthread->tid);
    mainthread->tid = thread->tid;
    id_table_set_thread(mainthread->tid, mainthread);


    hlist_add_head(&newproc->siblings_list_node, &proc->parent->childs);
    


    usproc_exit(proc, -1);
    hlist_del(&proc->siblings_list_node);
    proc->parent=NULL;
    proc->state =PCB_STATE_ZOMBIE;
    sti();
    
    _yield();
//shouldn't reach here


    

    return 0;
}