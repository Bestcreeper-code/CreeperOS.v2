#include "asm/asm.h"
#include "config.h"
#include "cpu/gdt.h"
#include "debug/Logger.h"
#include "defines/compiler_defs.h"
#include "defines/err_codes.h"
#include "drivers/drivers.h"
#include "memory/memory.h"
#include "scheduler/scheduler.h"
#include "vfs/fs.h"
#include "vfs/vfs.h"
#include "syscall.h"

#define MSR_STAR            0xC0000081
#define MSR_LSTAR           0xC0000082
#define MSR_SYSCALL_MASK    0xC0000084
#define MSR_EFER            0xC0000080

extern void _syscall_entry();

int syscall_init() {

    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);

    uint64_t star = ((uint64_t)(USER_CODE_SEGMENT - 16) << 48) |
                    ((uint64_t)(KERNEL_CODE_SEGMENT)     << 32);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)_syscall_entry);
    wrmsr(MSR_SYSCALL_MASK, (1 << 9) | (1 << 10)); // IF | DF

    // void _int80_handler();
    // setup_interrupt_vector(0x80, _int80_handler, IRQ_FLAG_USER );
    return 0;
}
REGISTER_DRIVER_CORE(syscalls, syscall_init);


int syscall_handler(
    register_t rax, //syscall number
    register_t rdi, //arg1
    register_t rsi, //arg2
    register_t rdx, //arg3
    register_t r10, //arg4
    register_t r8,  //arg5
    register_t r9,  //arg6
    register_t rsp  //stack ptr
) {
// #if (SYSCALL_DEBUG)
//     Sys_Debug("syscall: rax=%#lx, rdi=%#lx, rsi=%#lx, rdx=%#lx, r10=%#lx, r8=%#lx, r9=%#lx \n",
//         rax, rdi, rsi, rdx, r10, r8, r9);
// #endif

    switch (rax) {

        case 0: // sys_read
            return sys_read(rdi, rsi, rdx);

        case 1: // sys_write
            return sys_write(rdi, rsi, rdx);

        case 2: // sys_open
            return sys_open(rdi, rsi, rdx);

        case 3: // sys_close
            return sys_close(rdi);

        case 24: // sys_sched_yield
            // yield_core(rsp);
            return 0;

        case 60: // sys_exit
            sys_exit(rdi, rsp);
            //i swear... if that returns there is a big issue
            return 0;

        case 83: // sys_mkdir
            return sys_mkdir(rdi, rsi);

        case 84: // sys_rmdir
            return sys_rmdir(rdi);

        case 85: // sys_creat
            return sys_create(rdi, rsi);

        case 87: // sys_unlink
            return sys_unlink(rdi);

        default:
#if (SYSCALL_DEBUG)
            Sys_Error("Unknown syscall: %lx\n", rax);
#endif
            return -E_INVAL;
    }
}





int sys_open(
    register_t filename,
    register_t flags,
    register_t mode
) {
#if (SYSCALL_DEBUG)
    Sys_Debug("open called: filename=%p, flags=%x, mode=%o\n",
        (void*)filename, (unsigned)flags, (unsigned)mode);
#endif

    if (!filename)
        return -E_INVAL;

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)filename, sizeof(kpath));

    struct inode *start = path_resolve_start(kpath);
    if (!start)
        return -E_INVAL;

    
    file* table = (file*)_scheduler_current_process->opened_file_table;
    int fd = -1;
    
    for (int i = 3; i < DEFAULT_FILE_TABLE_ENTRIES; i++) {
        if (!table[i].f_inode) { fd = i; break; }
    }
    if (fd < 0)
        return -E_INVAL;

    struct dentry* d = kpath_lookup(start, kpath);

    if (!d) {
        if (!(flags & O_CREAT))
            return -E_INVAL;

        int ret = kpath_create(start, kpath, (umode_t)mode | S_IFREG, false);
        if (ret < 0)
            return ret;

        d = kpath_lookup(start, kpath);
        if (!d)
            return -E_INVAL;
    }

    table[fd].f_inode      = d->inode;
    table[fd].f_pos        = 0;
    table[fd].f_flags      = (uint32_t)flags;
    table[fd].f_ops        = d->inode->i_fop;
    table[fd].private_data = NULL;

    atomic_fetch_add(&d->inode->i_count, 1);

    if (table[fd].f_ops && table[fd].f_ops->open) {
        int ret = table[fd].f_ops->open(d->inode, &table[fd]);
        if (ret < 0) {
            atomic_fetch_sub(&d->inode->i_count, 1);
            table[fd].f_inode = NULL;
            table[fd].f_ops   = NULL;
            return ret;
        }
    }

    return fd;
}


ssize_t sys_read(
    register_t fd,
    register_t buf,
    register_t count
) {
#if (SYSCALL_DEBUG)
    Sys_Debug("read called: fd=%d, buf=%p, count=%ld\n",
        (int)fd, (void*)buf, count);
#endif

    if (!buf)
        return -E_INVAL;

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (fd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;

    file *fil = &((file*)_scheduler_current_process->opened_file_table)[fd];

    if (!fil->f_inode || !fil->f_ops || !fil->f_ops->read)
        return -E_INVAL;

    char *kbuf = kmalloc(count);
    if (!kbuf)
        return -E_NOMEM;

    int ret = -E_ACCES;

    unsigned int access = fil->f_flags & O_ACCESS;
    if (access == O_RD || access == O_RW)
        ret = fil->f_ops->read(fil, kbuf, count, &fil->f_pos);

    if (ret > 0) {
        if (copy_to_user((void __user*)buf, kbuf, ret) != 0) {
            kfree(kbuf);
            return -E_FAULT;
        }
    }

    kfree(kbuf);

    return ret;
}

int sys_write(
    register_t fd,
    register_t buf,
    register_t count
) {
#if (SYSCALL_DEBUG)
    Sys_Debug("write called: fd=%d, buf=%p, count=%ld\n",
        (int)fd, (void*)buf, count);
#endif

    if (!buf)
        return -E_INVAL;

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (fd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;
    
    if(fd==1) {
        Sys_Warning("[print] %.*s",(int)count, (char*)buf);
        return count;
    }
    file *fil = &((file*)_scheduler_current_process->opened_file_table)[fd];

    if (!fil->f_inode || !fil->f_ops || !fil->f_ops->write)
        return -E_INVAL;

    char *kbuf = kmalloc(count);
    if (!kbuf)
        return -E_NOMEM;

    if (copy_from_user(kbuf, (const void __user*)buf, count) != 0) {
        kfree(kbuf);
        return -E_FAULT;
    }

    int ret = -E_ACCES;

    unsigned int access = fil->f_flags & O_ACCESS;
    if (access == O_WR || access == O_RW)
        ret = fil->f_ops->write(fil, kbuf, count, &fil->f_pos);

    kfree(kbuf);

    return ret;
}

int sys_close(register_t fd) {
#if (SYSCALL_DEBUG)
    Sys_Debug("close called: fd=%d\n", (int)fd);
#endif

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (fd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;

    file *fil = &((file*)_scheduler_current_process->opened_file_table)[fd];

    if (!fil->f_inode)
        return -E_INVAL;

    int ret = 0;
    if (fil->f_ops && fil->f_ops->release)
        ret = fil->f_ops->release(fil->f_inode, fil);

    if (fil->f_inode)
        atomic_fetch_sub(&fil->f_inode->i_count, 1);

    fil->f_inode     = NULL;
    fil->f_ops       = NULL;
    fil->f_pos       = 0;
    fil->f_flags     = 0;
    fil->private_data = NULL;

    return ret;
}

int sys_mkdir(register_t path, register_t mode) {
#if (SYSCALL_DEBUG)
    Sys_Debug("mkdir called: path=%p, mode=%o\n", (void*)path, (unsigned)mode);
#endif

    if (!path)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)path, sizeof(kpath));
    

    struct inode *start = path_resolve_start(kpath);
    if (!start)
        return -E_INVAL;

    return kpath_mkdir(start, kpath, (umode_t)mode);
}

int sys_create(register_t path, register_t mode) {
#if (SYSCALL_DEBUG)
    Sys_Debug("create called: path=%p, mode=%o\n", (void*)path, (unsigned)mode);
#endif

    if (!path)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)path, sizeof(kpath));
    

    struct inode* start = path_resolve_start(kpath);
    if (!start)
        return -E_INVAL;

    return kpath_create(start, kpath, (umode_t)mode, false);
}

int sys_rmdir(register_t path) {
#if (SYSCALL_DEBUG)
    Sys_Debug("rmdir called: path=%p\n", (void*)path);
#endif

    if (!path)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)path, sizeof(kpath));
    
    struct inode* start = path_resolve_start(kpath);
    if (!start)
        return -E_INVAL;

    char* parent_path;
    char* name;
    int err = split_path(kpath, &parent_path, &name);
    if (err)
        return err;

    int ret = kpath_rmdir(start, parent_path, name);

    kfree(parent_path);
    kfree(name);
    return ret;
}

int sys_unlink(register_t path) {
#if (SYSCALL_DEBUG)
    Sys_Debug("unlink called: path=%p\n", (void *)path);
#endif

    if (!path)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)path, sizeof(kpath));
    

    struct inode *start = path_resolve_start(kpath);
    if (!start)
        return -E_INVAL;

    char *parent_path, *name;
    int err = split_path(kpath, &parent_path, &name);
    if (err)
        return err;

    int ret = path_unlink(start, parent_path, name);

    kfree(parent_path);
    kfree(name);
    return ret;
}





void sys_exit(register_t code, register_t rsp) {
    _scheduler_current_process->exit_code = code;
    _scheduler_current_process->state     = PCB_STATE_ZOMBIE;

    yield_core(rsp);
}