#include "elf.h"
#include "config.h"
#include "debug/Logger.h"
#include "defines/err_codes.h"
#include "vfs/vfs.h"
#include "vfs/fs.h"
#include "memory/pmm.h"
#include "mm/vmm_arch.h"
#include "memops.h"

#include <stdint.h>


int elf64_validate(elf64_ehdr* eh)
{
    if (eh->e_ident[EI_MAG0] != ELFMAG0 ||
        eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 ||
        eh->e_ident[EI_MAG3] != ELFMAG3)
        return -E_NOEXEC;

    if (eh->e_ident[EI_CLASS] != ELFCLASS64)
        return -E_NOEXEC;

    if (eh->e_ident[EI_DATA] != ELFDATA2LSB)
        return -E_NOEXEC;

    if (eh->e_machine != EM_X86_64)
        return -E_NOEXEC;

    if (eh->e_type != ET_EXEC)
        return -E_NOEXEC;

    if (eh->e_phnum == 0 || eh->e_phoff == 0)
        return -E_NOEXEC;

    return 0;
}

pid_t load_elf_from_vfs(const char* vfs_path, char** argv, char** envp)
{
    physptr_t pml4 = pmm_alloc_pages_zeroed(1);
    RET_IF(!pml4, -E_NOMEM);

#if ELF_DEBUG
    Sys_Debug("pml4=%p\n", (void*)pml4);
#endif

    dentry* dent = kpath_lookup(root_dentry->inode, vfs_path);
    RET_IF(!dent, -E_NOENT);

#if ELF_DEBUG
    Sys_Debug("dentry=%p inode=%p\n", dent, dent->inode);
#endif

    size_t image_size = dent->inode->i_size;
    size_t raw_pages = (image_size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    if (raw_pages == 0) raw_pages = 1;

    physptr_t raw_image = pmm_alloc_pages_zeroed(raw_pages);
    RET_IF(!raw_image, -E_NOMEM);

#if ELF_DEBUG
    Sys_Debug("raw_image=%p (%zu bytes, %zu pages)\n",
              (void*)raw_image, image_size, raw_pages);
#endif

    file f = {0};

    int ret = dent->inode->i_fop->open(dent->inode, &f);
    RET_IF(ret < 0, ret);

    loff_t off = 0;

    ssize_t rd = f.f_ops->read(
        &f,
        PHYS_2_HHDM(raw_image),
        image_size,
        &off
    );

    if (rd != (ssize_t)image_size) {
        pmm_free_pages(raw_image, raw_pages);
        pmm_free_pages(pml4, 1);
        return -E_IO;
    }

#if ELF_DEBUG
    Sys_Debug("loaded %ld bytes\n", rd);
#endif

    uint8_t* raw = (uint8_t*)PHYS_2_HHDM(raw_image);
    elf64_ehdr* ehdr = (elf64_ehdr*)raw;

    ret = elf64_validate(ehdr);
    if (ret < 0) {
        pmm_free_pages(raw_image, raw_pages);
        pmm_free_pages(pml4, 1);
        return ret;
    }

#if ELF_DEBUG
    Sys_Debug("elf entry=%p phnum=%u phoff=%llu\n",
              (void*)ehdr->e_entry, ehdr->e_phnum,
              (unsigned long long)ehdr->e_phoff);
#endif

    elf64_phdr* phdrs = (elf64_phdr*)(raw + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        elf64_phdr* ph = &phdrs[i];

        if (ph->p_type != PT_LOAD)
            continue;

        uintptr_t vaddr_aligned = ph->p_vaddr & ~(PAGE_SIZE_4K - 1);
        uintptr_t page_offset   = ph->p_vaddr & (PAGE_SIZE_4K - 1);
        size_t seg_span         = page_offset + ph->p_memsz;
        size_t seg_pages        = (seg_span + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
        if (seg_pages == 0) seg_pages = 1;

        physptr_t seg_phys = pmm_alloc_pages_zeroed(seg_pages);
        if (!seg_phys) {
            pmm_free_pages(raw_image, raw_pages);
            pmm_free_pages(pml4, 1);
            return -E_NOMEM;
        }

        if (ph->p_filesz > 0) {
            memcpy(
                (uint8_t*)PHYS_2_HHDM(seg_phys) + page_offset,
                raw + ph->p_offset,
                ph->p_filesz
            );
        }

#if ELF_DEBUG
        Sys_Debug("PT_LOAD[%u]: vaddr=%p filesz=%llu memsz=%llu flags=%c%c%c pages=%zu phys=%p\n",
                  i, (void*)ph->p_vaddr,
                  (unsigned long long)ph->p_filesz,
                  (unsigned long long)ph->p_memsz,
                  (ph->p_flags & PF_R) ? 'R' : '-',
                  (ph->p_flags & PF_W) ? 'W' : '-',
                  (ph->p_flags & PF_X) ? 'X' : '-',
                  seg_pages, (void*)seg_phys);
#endif
 
        uint64_t map_flags = PTE_PRESENT | PTE_USER | PTE_LOCAL_OWNED;

        if (ph->p_flags & PF_W)
            map_flags |= PTE_WRITABLE;

        if (!(ph->p_flags & PF_X))
            map_flags |= PTE_NX;

        map_4k_pages(
            PHYS_2_HHDM(pml4),
            vaddr_aligned,
            seg_phys,
            seg_pages,
            map_flags
        );
    }

#if ELF_DEBUG
    dump_userspace_mappings(PHYS_2_HHDM(pml4));
#endif

    pmm_free_pages(raw_image, raw_pages);

    linked_pcb* pcb = us_task_start(
        (void*)ehdr->e_entry,
        vfs_path,
        pml4,
        argv,
        envp
    );
    pcb->cwd_i = dent->parent->inode;

    
    pcb->state = PCB_STATE_RUNNING;
    RET_IF(!pcb, -E_NOMEM);

    return pcb->pid;
}