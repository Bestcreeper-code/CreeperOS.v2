#include "execve.h"
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

static inline uint64_t* _table_virt(uint64_t entry)
{
    return (uint64_t*)PHYS_2_HHDM(entry & PTE_ADDR_MASK);
}

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

static size_t _argv_len(char** arr)
{
    size_t n = 0;
    if (arr) while (arr[n]) n++;
    return n;
}

static bool _exec_setup_user_args(uint64_t* new_pml4, char** argv, char** envp,
                                   uint64_t* out_argc,
                                   uint64_t* out_argv_uva,
                                   uint64_t* out_envp_uva)
{
    size_t argc = _argv_len(argv);
    size_t envc = _argv_len(envp);

    size_t argv_ptrs_size = (argc + 1) * sizeof(char*);
    size_t envp_ptrs_size = (envc + 1) * sizeof(char*);

    size_t str_bytes = 0;
    for (size_t i = 0; i < argc; i++) str_bytes += strlen(argv[i]) + 1;
    for (size_t i = 0; i < envc; i++) str_bytes += strlen(envp[i]) + 1;

    size_t total = argv_ptrs_size + envp_ptrs_size + str_bytes;
    size_t page_count = (total + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    if (page_count == 0) page_count = 1;
    if (page_count > USER_ARGS_MAX_PAGES) return false;

    physptr_t args_pages = pmm_alloc_pages(page_count);
    if (!args_pages) return false;

    map_4k_pages(new_pml4, USER_ARGS_USPACE_ADDR, args_pages, page_count,
                 PTE_USER | PTE_WRITABLE | PTE_NX | PTE_LOCAL_OWNED);

    uint8_t* kbase = (uint8_t*)PHYS_2_HHDM(args_pages);
    memset(kbase, 0, page_count * PAGE_SIZE_4K);

    uint64_t* k_argv_ptrs = (uint64_t*)kbase;
    uint64_t* k_envp_ptrs = (uint64_t*)(kbase + argv_ptrs_size);
    uint8_t*  k_strings   = kbase + argv_ptrs_size + envp_ptrs_size;
    uint64_t  strings_uva = USER_ARGS_USPACE_ADDR + argv_ptrs_size + envp_ptrs_size;
    size_t    str_off     = 0;

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

    *out_argc     = argc;
    *out_argv_uva = USER_ARGS_USPACE_ADDR;
    *out_envp_uva = USER_ARGS_USPACE_ADDR + argv_ptrs_size;
    return true;
}

int exec_elf_from_vfs(linked_pcb* proc, const char* vfs_path,
                       char** argv, char** envp, uint64_t rsp)
{
    if (!proc || !proc->cr3 || proc->cr3 == kernel_pagedir_phys)
        return -E_INVAL;

    if (!rsp)
        return -E_INVAL;
    Sys_Info("%lx \n",pmm_get_free_pages());
    
    process_stack_frame* frame = (process_stack_frame*)rsp;
    
    dentry* dent = kpath_lookup(root_dentry->inode, vfs_path);
    RET_IF(!dent, -E_NOENT);
    
    size_t image_size = dent->inode->i_size;
    size_t raw_pages = (image_size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    if (raw_pages == 0) raw_pages = 1;
    
    Sys_Info("asking for %lx pages for exec \n",raw_pages);
    physptr_t raw_image = pmm_alloc_pages(raw_pages);
    if(!raw_image){
        
        return -E_NOMEM;
    } 


    file f = {0};
    int ret = dent->inode->i_fop->open(dent->inode, &f);
    if (ret < 0) {
        pmm_free_pages(raw_image, raw_pages);
        return ret;
    }

    loff_t off = 0;
    ssize_t rd = f.f_ops->read(&f, PHYS_2_HHDM(raw_image), image_size, &off);
    if (rd != (ssize_t)image_size) {
        pmm_free_pages(raw_image, raw_pages);
        return -E_IO;
    }

    uint8_t* raw = (uint8_t*)PHYS_2_HHDM(raw_image);
    elf64_ehdr* ehdr = (elf64_ehdr*)raw;

    ret = elf64_validate(ehdr);
    if (ret < 0) {
        pmm_free_pages(raw_image, raw_pages);
        return ret;
    }

#if EXEC_DEBUG
    Sys_Debug("execve: '%s' entry=%p phnum=%u\n",
              vfs_path, (void*)ehdr->e_entry, ehdr->e_phnum);
#endif

    // build the new address space fully before touching the old one

    physptr_t new_pml4_phys = pmm_alloc_pages_zeroed(1);
    if (!new_pml4_phys) {
#if EXEC_DEBUG
        Sys_Error("execve: failed allocating PML4\n");
#endif
        pmm_free_pages(raw_image, raw_pages);
        return -E_NOMEM;
    }
    uint64_t* new_pml4 = PHYS_2_HHDM(new_pml4_phys);
    uint64_t* old_pml4 = PHYS_2_HHDM(proc->cr3);

    for (int i = 255; i < 512; i++)
        new_pml4[i] = ((uint64_t*)PHYS_2_HHDM(kernel_pagedir_phys))[i];

    
    physptr_t ft_phys = 0;
    if (proc->opened_file_table) {
        bool owned = false;
        ft_phys = _walk_leaf_phys(old_pml4, DEFAULT_FILE_TABLE_START, &owned);
        if (!ft_phys) {
            Sys_Error("execve: '%s': couldn't locate file table phys\n", vfs_path);
            vmm_pagetable_free_owned(new_pml4_phys);
            pmm_free_pages(raw_image, raw_pages);
            return -E_NOMEM;
        }

        map_4k_pages(new_pml4, DEFAULT_FILE_TABLE_START, ft_phys,
                     DEFAULT_FILE_TABLE_PAGES,
                     PTE_WRITABLE | PTE_LOCAL_OWNED);
    }

    //map PT_LOAD segments
    elf64_phdr* phdrs = (elf64_phdr*)(raw + ehdr->e_phoff);
    bool load_failed = false;

    for (uint16_t i = 0; i < ehdr->e_phnum && !load_failed; i++) {
        elf64_phdr* ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;

        uintptr_t vaddr_aligned = ph->p_vaddr & ~(PAGE_SIZE_4K - 1);
        uintptr_t page_offset   = ph->p_vaddr & (PAGE_SIZE_4K - 1);
        size_t seg_span         = page_offset + ph->p_memsz;
        size_t seg_pages        = (seg_span + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
        if (seg_pages == 0) seg_pages = 1;

        physptr_t seg_phys = pmm_alloc_pages_zeroed(seg_pages);
        if (!seg_phys) {
#if EXEC_DEBUG
            Sys_Error("execve: failed to allocate PT_LOAD %u (%zu pages)\n",
                        i, seg_pages);
#endif
            load_failed = true;
            break;
        }

        if (ph->p_filesz > 0) {
            memcpy((uint8_t*)PHYS_2_HHDM(seg_phys) + page_offset,
                   raw + ph->p_offset, ph->p_filesz);
        }

        uint64_t map_flags = PTE_PRESENT | PTE_USER | PTE_LOCAL_OWNED;
        if (ph->p_flags & PF_W) map_flags |= PTE_WRITABLE;
        if (!(ph->p_flags & PF_X)) map_flags |= PTE_NX;

        map_4k_pages(new_pml4, vaddr_aligned, seg_phys, seg_pages, map_flags);
    }

    if (load_failed) {
        vmm_pagetable_free_owned(new_pml4_phys);
        pmm_free_pages(raw_image, raw_pages);
        return -E_NOMEM;
    }

    pmm_free_pages(raw_image, raw_pages);

    //new user stack
    page_index us_stack_pages = pmm_alloc_pages(DEFAULT_STACK_PAGE_AMOUNT);
    if (!us_stack_pages) {
#if EXEC_DEBUG
        Sys_Error("execve: failed allocating user stack\n");
#endif
        vmm_pagetable_free_owned(new_pml4_phys);
        return -E_NOMEM;
    }
    uintptr_t us_stack_bott = us_stack_pages;
    for (int i = 0; i < DEFAULT_STACK_PAGE_AMOUNT; i++) {
        uintptr_t page_addr = us_stack_bott + i * PAGE_SIZE_4K;
        map_4k(new_pml4,
               STACK_UPPER_USPACE_ADDR - (DEFAULT_STACK_PAGE_BYTES - i * PAGE_SIZE_4K),
               page_addr, PTE_WRITABLE | PTE_NX | PTE_USER | PTE_LOCAL_OWNED);
    }

    //new heap
    physptr_t heap_pages = pmm_alloc_pages(DEFAULT_USER_HEAP_PAGES);
    if (!heap_pages) {
#if EXEC_DEBUG
        Sys_Error("execve: failed allocating heap\n");
#endif
        vmm_pagetable_free_owned(new_pml4_phys);
        return -E_NOMEM;
    }
    map_4k_pages(new_pml4, DEFAULT_USER_HEAP_START, heap_pages,
                 DEFAULT_USER_HEAP_PAGES, PTE_USER | PTE_WRITABLE | PTE_LOCAL_OWNED);

    //argv/envp
    uint64_t u_argc = 0, u_argv = 0, u_envp = 0;
    if (!_exec_setup_user_args(new_pml4, argv, envp, &u_argc, &u_argv, &u_envp)) {
#if EXEC_DEBUG
        Sys_Error("execve: failed setting up argv/envp\n");
#endif    
        vmm_pagetable_free_owned(new_pml4_phys);
        return -E_NOMEM;
    }

#if EXEC_DEBUG
    Sys_Debug("execve: new pml4=%p ready, entry=%p\n",
              (void*)new_pml4_phys, (void*)ehdr->e_entry);
#endif


    if (proc->opened_file_table) {
        
        for (int i = 0; i < DEFAULT_FILE_TABLE_PAGES; i++)
            vmm_unmap_page(ADDR_TO_PAGE(DEFAULT_FILE_TABLE_START) + i);
    }

    physptr_t old_pml4_phys = proc->cr3;

    
    vmm_pagetable_free_owned(old_pml4_phys);

    proc->cr3 = new_pml4_phys;
    proc->heap.lower  = DEFAULT_USER_HEAP_START;
    proc->heap.higher = DEFAULT_USER_HEAP_END;
    proc->heap.size   = DEFAULT_USER_HEAP_PAGES * PAGE_SIZE_4K;
    proc->user_stack  = (stack_t){
        .bottom = us_stack_bott,
        .top    = us_stack_bott + DEFAULT_STACK_PAGE_BYTES,
        .size   = DEFAULT_STACK_PAGE_BYTES
    };
    

    char* old_name = proc->name;
    proc->name = strdup(vfs_path);
    if (old_name) kfree(old_name);

    __asm__ volatile ("mov %0, %%cr3" :: "r"(proc->cr3) : "memory");

    
    frame->r15 = frame->r14 = frame->r13 = frame->r12 = 0;
    frame->r11 = frame->r10 = frame->r9  = frame->r8  = 0;
    frame->rbp = frame->rbx = frame->rcx = frame->rax = 0;

    frame->rdi = u_argc;
    frame->rsi = u_argv;
    frame->rdx = u_envp;

    frame->rip    = ehdr->e_entry;
    frame->rflags = 0x202;
    frame->rsp    = STACK_UPPER_USPACE_ADDR;


    Sys_Success("execve: pid %u now running '%s'\n", proc->pid, vfs_path);

    return 0;
}