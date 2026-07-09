#include "bin_exec.h"
#include "defines/err_codes.h"
#include "vfs/vfs.h"
#include "vfs/fs.h"

#include <stdint.h>
int bin_exec_raw(
    const char* path,
    uintptr_t entry_addr,
    char** argv,
    char** envp
)
{
    physptr_t pml4 = pmm_alloc_pages_zeroed(1);
    RET_IF(!pml4, -E_NOMEM);

#if EXECUTABLE_DEBUG
    Sys_Debug("pml4=%p\n", (void*)pml4);
#endif

    dentry* dent = kpath_lookup(root_dentry->inode, path);
    RET_IF(!dent, -E_NOENT);

#if EXECUTABLE_DEBUG
    Sys_Debug("dentry=%p inode=%p\n", dent, dent->inode);
#endif

    physptr_t image = pmm_alloc_zeroed();
    RET_IF(!image, -E_NOMEM);

#if EXECUTABLE_DEBUG
    Sys_Debug("image=%p\n", (void*)image);
#endif

    file f = {0};

    int ret = dent->inode->i_fop->open(dent->inode, &f);
    RET_IF(ret < 0, ret);

    loff_t off = 0;

    ssize_t rd = f.f_ops->read(
        &f,
        PHYS_2_HHDM(image),
        dent->inode->i_size,
        &off
    );

    if (rd != (ssize_t)dent->inode->i_size)
        return -E_IO;

#if EXECUTABLE_DEBUG
    Sys_Debug("loaded %ld bytes\n", rd);
#endif

    map_4k(
        PHYS_2_HHDM(pml4),
        entry_addr,
        image,
        PTE_PRESENT |
        PTE_USER |
        PTE_WRITABLE |
        PTE_LOCAL_OWNED
    );

#if EXECUTABLE_DEBUG
    dump_userspace_mappings(PHYS_2_HHDM(pml4));
#endif

    us_task_start(
        (void*)entry_addr,
        path,
        pml4,
        argv,
        envp
    );

    return 0;
}