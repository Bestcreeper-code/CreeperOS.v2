#include "arch/vmm.h"
#include "asm/asm.h"
#include "config.h"
#include "cpu/gdt.h"
#include "debug/Logger.h"
#include "defines/compiler_defs.h"
#include "defines/err_codes.h"
#include "defines/types.h"
#include "drivers/drivers.h"
#include "executable/elf/execve.h"
#include "executable/elf/fork.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include "mm/vmm_arch.h"

#include "scheduler/scheduler.h"
#include "string/string.h"

#include "syscall.h"
#include "vfs/fs.h"
#include "vfs/vfs.h"
#include <stddef.h>
#include <stdint.h>

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




static char** _copy_user_strv(const __user char* const uarr[])
{
    if (!uarr) return NULL;

    __user char* uptrs[EXEC_ARGV_MAX];
    size_t n = 0;

    for (; n < EXEC_ARGV_MAX; n++) {
        __user char* p;
        if (copy_from_user(&p, (const __user void*)&uarr[n], sizeof(p)) != 0)
            return NULL;
        if (!p) break;
        uptrs[n] = p;
    }

    char** out = kmalloc((n + 1) * sizeof(char*));
    if (!out) return NULL;

    for (size_t i = 0; i < n; i++) {
        char* kstr = kmalloc(EXEC_ARG_MAXLEN);
        if (!kstr) {
            for (size_t j = 0; j < i; j++) kfree(out[j]);
            kfree(out);
            return NULL;
        }
        copy_from_user(kstr, (const __user void*)uptrs[i], EXEC_ARG_MAXLEN);
        kstr[EXEC_ARG_MAXLEN - 1] = '\0';
        out[i] = kstr;
    }
    out[n] = NULL;
    return out;
}

static void _free_strv(char** v)
{
    if (!v) return;
    for (size_t i = 0; v[i]; i++) kfree(v[i]);
    kfree(v);
}

static struct inode* resolve_at(int dfd, const char* kpath)
{
    if (kpath[0] == '/')
        return path_resolve_start(kpath);

    if (dfd == AT_FDCWD)
        return _scheduler_current_process->cwd_i;

    if (dfd < 0 || dfd >= DEFAULT_FILE_TABLE_ENTRIES)
        return NULL;

    if (!_scheduler_current_process->opened_file_table)
        return NULL;

    file* table = (file*)_scheduler_current_process->opened_file_table;

    if (!table[dfd].f_inode)
        return NULL;

    if (!S_ISDIR(table[dfd].f_inode->i_mode))
        return NULL;

    return table[dfd].f_inode;
}

ssize_t syscall_handler(
    register_t rax, //syscall number
    register_t rdi, //arg1
    register_t rsi, //arg2
    register_t rdx, //arg3
    register_t r10, //arg4
    register_t r8,  //arg5
    register_t r9,  //arg6
    register_t rsp  //stack ptr
) {
#if (SYSCALL_DEBUG)
    Sys_Debug("syscall: rax=%#lx, rdi=%#lx, rsi=%#lx, rdx=%#lx, r10=%#lx, r8=%#lx, r9=%#lx, rsp=%#lx \n",
        rax, rdi, rsi, rdx, r10, r8, r9, rsp);
#endif

    switch (rax) {

        case 0: // sys_read
            return sys_read(rdi, (__user void*)rsi, rdx);

        case 1: // sys_write
            return sys_write(rdi, (const __user void*)rsi, rdx);

        case 2: // sys_open
            return sys_open((const __user char*)rdi, rsi, rdx);

        case 3: // sys_close
            return sys_close(rdi);

        case 8:
            return sys_lseek(rdi, rsi, rdx);
        
        case 12:
            return sys_brk(rdi);
        
        case 21: // sys_access
            return sys_access((const __user char*)rdi, rsi);

        case 24: // sys_sched_yield
            // yield_core(rsp);
            return 0;

        case 32: // sys_dup
            return sys_dup(rdi);

        case 33: // sys_dup2
            return sys_dup2(rdi, rsi);

        case 39:
            return sys_getpid();
            
        case 57:
            return sys_fork(rsp);
            
        case 59:
            return sys_execve(
                (const __user char* )rdi,
                (const __user char**)rsi,
                (const __user char**)rdx,
                rsp
            );
            
        case 60: // sys_exit
            sys_exit(rdi, rsp);
            //i swear... if that returns there is a big issue
            return 0;
            
        case 79:
            return (ssize_t)sys_getcwd((__user char*)rdi, rsi);

        case 80: // sys_chdir
            return sys_chdir((const __user char*)rdi);
            
        case 83: // sys_mkdir
            return sys_mkdir((const __user char*)rdi, rsi);

        case 84: // sys_rmdir
            return sys_rmdir((const __user char*)rdi);

        case 85: // sys_creat
            return sys_create((const __user char*)rdi, rsi);

        case 87: // sys_unlink
            return sys_unlink((const __user char*)rdi);
        case 158:
            return sys_arch_prctl(rdi, (unsigned long*) rsi);

        case 217: // sys_getdents64
            return sys_getdents64(rdi, (__user void*)rsi, rdx);
        
            
        case 257: // sys_openat
            return sys_openat(rdi, (const __user char*)rsi, rdx, r10);

        case 258: // sys_mkdirat
            return sys_mkdirat(rdi, (const __user char*)rsi, rdx);

        case 263: // sys_unlinkat
            return sys_unlinkat(rdi, (const __user char*)rsi, rdx);
        
        default:
#if (SYSCALL_DEBUG)
            Sys_Error("Unknown syscall: %lx\n", rax);
#endif
            return -E_INVAL;
    }
}





int sys_open(
    const __user char* filename,
    int flags,
    int mode
) {
#if (SYSCALL_DEBUG ==2)
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
    int fd,
    __user void* buf,
    size_t count
) {
#if (SYSCALL_DEBUG ==2)
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

    unsigned int access = fil->f_flags & O_ACCMODE;
    if (access == O_RDONLY || access == O_RDWR)
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

ssize_t sys_write(
    int fd,
    const __user void* buf,
    size_t count
) {
#if (SYSCALL_DEBUG ==2)
    Sys_Debug("write called: fd=%d, buf=%p, count=%ld\n",
        (int)fd, (void*)buf, count);
#endif
    if(fd==1) {
        Sys_log_NoPos("%.*s\n",(int)count, (char*)buf);
        return count;
    } else if(fd==2) {
        Sys_log_NoPos(ESC_RED"%.*s\n"ESC_RESET,(int)count, (char*)buf);
        return count;
    }

    if (!buf)
        return -E_INVAL;

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (fd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;
    
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

    unsigned int access = fil->f_flags & O_ACCMODE;
    if (access == O_WRONLY || access == O_RDWR)
        ret = fil->f_ops->write(fil, kbuf, count, &fil->f_pos);

    kfree(kbuf);

    return ret;
}


int sys_close(
    int fd
) {
#if (SYSCALL_DEBUG ==2)
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



loff_t sys_lseek(
    unsigned int fd,
    loff_t offset,
    unsigned int origin
) {
    if(fd<3) return -E_SPIPE;

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (fd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;

    file* fil = &((file*)_scheduler_current_process->opened_file_table)[fd];

    if (!fil->f_inode )
        return -E_INVAL;

    switch (origin) {
        case SEEK_SET:
            fil->f_pos = offset;
            return offset;
            
        case SEEK_CUR:
            fil->f_pos += offset;
            return fil->f_pos;
            
        case SEEK_END:
            fil->f_pos = fil->f_inode->i_size + offset;
            return fil->f_pos;
            
    }
}



uintptr_t sys_brk(size_t brk) {
    heap_t *heap = &_scheduler_current_process->heap;

    if (brk == 0)
        return heap->higher;

    if (brk < heap->lower)
        return -E_INVAL;

    uintptr_t old = heap->higher;

    uintptr_t old_page = (old + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
    uintptr_t new_page = (brk + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);

    if (new_page > old_page) {
        size_t pages = (new_page - old_page) / PAGE_SIZE_4K;

        uintptr_t phys = pmm_alloc_pages(pages);
        if (!phys)
            return -E_NOMEM;

        map_4k_pages(
            PHYS_2_HHDM(_scheduler_current_process->cr3),
            old_page,
            phys,
            pages,
            PTE_WRITABLE | PTE_USER
        );
    }
    else if (new_page < old_page) {
        //unmap later
    }

    heap->higher = brk;
    heap->size = heap->higher - heap->lower;

    return heap->higher;
}









pid_t sys_getpid() {
    return _scheduler_current_process->pid;
}


pid_t sys_fork(register_t rsp) {
    pid_t pid = fork_process(_scheduler_current_process, rsp);
    if(pid < 1) return -E_NOMEM;

    return pid;
}

int sys_execve(
    const __user char* filename,
    const __user char* argv[],
    const __user char* envp[],
    register_t rsp
) {
    if (!filename || !rsp)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)filename, sizeof(kpath));

    char** kargv = _copy_user_strv(argv);
    char** kenvp = _copy_user_strv(envp);

    int ret = exec_elf_from_vfs(_scheduler_current_process, kpath, kargv, kenvp, rsp);

    _free_strv(kargv);
    _free_strv(kenvp);

    return ret;
}


void sys_exit(  
    int code,
    register_t rsp
) {
    _scheduler_current_process->exit_code = code;
    _scheduler_current_process->state     = PCB_STATE_DEAD;

    yield_core(rsp);
}


char* sys_getcwd(
    __user char* buf,
    unsigned long size
) {
    if (!buf || size == 0)
        return (char*)-E_INVAL;

    if (!_scheduler_current_process->cwd_i)
        return (char*)-E_INVAL;

    char *path = kpath_reverse(_scheduler_current_process->cwd_i->i_dentry);
    if (!path)
        return (char*)-E_NOMEM;

    size_t len = strlen(path) + 1;

    if (len > size) {
        kfree(path);
        return (char*)-E_RANGE;
    }

    if (copy_to_user(buf, path, len) != 0) {
        kfree(path);
        return (char*)-E_FAULT;
    }

    kfree(path);
    return (char*)buf;
}


int sys_mkdir(
    const __user char* path,
    int mode
) {
#if (SYSCALL_DEBUG ==2)
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


int sys_create(
    const __user char* path,
    int mode
) {
#if (SYSCALL_DEBUG ==2)
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

int sys_rmdir(
    const __user char* path
) {
#if (SYSCALL_DEBUG ==2)
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

int sys_unlink(
    const __user char* path
) {
#if (SYSCALL_DEBUG ==2)
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






int sys_chdir(
    const __user char* path
) {
#if (SYSCALL_DEBUG == 2)
    Sys_Debug("chdir called: path=%p\n", (void*)path);
#endif

    if (!path)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)path, sizeof(kpath));

    struct inode* start = path_resolve_start(kpath);
    if (!start)
        return -E_INVAL;

    struct dentry* d = kpath_lookup(start, kpath);
    if (!d)
        return -E_NOENT;

    if (!S_ISDIR(d->inode->i_mode))
        return -E_NOTDIR;

    _scheduler_current_process->cwd_i = d->inode;

    return 0;
}


int sys_dup(
    int oldfd
) {
    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (oldfd < 0 || oldfd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;

    file* table = (file*)_scheduler_current_process->opened_file_table;

    if (!table[oldfd].f_inode)
        return -E_INVAL;

    int newfd = -1;
    for (int i = 0; i < DEFAULT_FILE_TABLE_ENTRIES; i++) {
        if (!table[i].f_inode) { newfd = i; break; }
    }
    if (newfd < 0)
        return -E_MFILE;

    table[newfd] = table[oldfd];
    atomic_fetch_add(&table[oldfd].f_inode->i_count, 1);

    return newfd;
}


int sys_dup2(
    int oldfd,
    int newfd
) {
    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (oldfd < 0 || oldfd >= DEFAULT_FILE_TABLE_ENTRIES ||
        newfd < 0 || newfd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;

    file* table = (file*)_scheduler_current_process->opened_file_table;

    if (!table[oldfd].f_inode)
        return -E_INVAL;

    if (oldfd == newfd)
        return newfd;

    
    if (table[newfd].f_inode) {
        if (table[newfd].f_ops && table[newfd].f_ops->release)
            table[newfd].f_ops->release(table[newfd].f_inode, &table[newfd]);
        atomic_fetch_sub(&table[newfd].f_inode->i_count, 1);
    }

    table[newfd] = table[oldfd];
    atomic_fetch_add(&table[oldfd].f_inode->i_count, 1);

    return newfd;
}

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

int sys_access(
    const __user char* path,
    int mode
) {
#if (SYSCALL_DEBUG == 2)
    Sys_Debug("access called: path=%p, mode=%o\n", (void*)path, (unsigned)mode);
#endif

    if (!path)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)path, sizeof(kpath));

    struct inode* start = path_resolve_start(kpath);
    if (!start)
        return -E_INVAL;

    struct dentry* d = kpath_lookup(start, kpath);
    if (!d)
        return -E_NOENT;

    struct inode* inode = d->inode;

    
    if (mode == F_OK)
        return 0;

    kuid_t uid = _scheduler_current_process->uid;
    kgid_t gid = _scheduler_current_process->gid;

    umode_t perm_bits;

    if (uid == 0) {
        
        perm_bits = S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH;
    } else if (uid == inode->i_uid) {
        perm_bits = (inode->i_mode & S_IRWXU) >> 6;
        perm_bits <<= 6; 
        perm_bits = (inode->i_mode & S_IRWXU);
    } else if (gid == inode->i_gid) {
        perm_bits = (inode->i_mode & S_IRWXG) << 3; 
    } else {
        perm_bits = (inode->i_mode & S_IRWXO) << 6; 
    }

    

    if ((mode & R_OK) && !(perm_bits & S_IRUSR))
        return -E_ACCES;

    if ((mode & W_OK) && !(perm_bits & S_IWUSR))
        return -E_ACCES;

    if ((mode & X_OK) && !(perm_bits & S_IXUSR))
        return -E_ACCES;

    return 0;
}

long sys_arch_prctl(
    int code,
    unsigned long* addr
) {
    switch (code) {
        case ARCH_PRCTL_SET_FS:{
            wrmsr(MSR_FS_BASE, (uint64_t)addr);
            return 0;
        }
    
    }
    return -E_INVAL;
}








ssize_t sys_getdents64(
    unsigned int fd,
    __user void* dirp,
    size_t count
) {
#if (SYSCALL_DEBUG == 2)
    Sys_Debug("getdents64 called: fd=%u, dirp=%p, count=%zu\n", fd, dirp, count);
#endif

    if (!dirp || count == 0)
        return -E_INVAL;

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    if (fd >= DEFAULT_FILE_TABLE_ENTRIES)
        return -E_INVAL;

    file* fil = &((file*)_scheduler_current_process->opened_file_table)[fd];

    if (!fil->f_inode)
        return -E_INVAL;

    char* kbuf = kmalloc(count);
    if (!kbuf)
        return -E_NOMEM;

    ssize_t ret = vfs_getdents64(fil, kbuf, count);

    if (ret > 0 && copy_to_user(dirp, kbuf, ret) != 0) {
        kfree(kbuf);
        return -E_FAULT;
    }

    kfree(kbuf);
    return ret;
}






int sys_openat(
    int dfd,
    const __user char* filename,
    int flags,
    int mode
) {
#if (SYSCALL_DEBUG ==2)
    Sys_Debug("openat called: dfd=%d, filename=%p, flags=%x, mode=%o\n",
        dfd, (void*)filename, (unsigned)flags, (unsigned)mode);
#endif

    if (!filename)
        return -E_INVAL;

    if (!_scheduler_current_process->opened_file_table)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)filename, sizeof(kpath));

    struct inode *start = resolve_at(dfd, kpath);
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






int sys_mkdirat(
    int dfd,
    const __user char* path,
    int mode
) {
#if (SYSCALL_DEBUG ==2)
    Sys_Debug("mkdirat called: dfd=%d, path=%p, mode=%o\n", dfd, (void*)path, (unsigned)mode);
#endif

    if (!path)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)path, sizeof(kpath));

    struct inode *start = resolve_at(dfd, kpath);
    if (!start)
        return -E_INVAL;

    return kpath_mkdir(start, kpath, (umode_t)mode);
}

int sys_unlinkat(
    int dfd,
    const __user char* pathname,
    int flag
) {
#if (SYSCALL_DEBUG ==2)
    Sys_Debug("unlinkat called: dfd=%d, pathname=%p, flag=%x\n", dfd, (void*)pathname, (unsigned)flag);
#endif

    if (!pathname)
        return -E_INVAL;

    char kpath[PATH_MAX];
    copy_from_user(kpath, (const __user void*)pathname, sizeof(kpath));

    struct inode *start = resolve_at(dfd, kpath);
    if (!start)
        return -E_INVAL;

    char *parent_path, *name;
    int err = split_path(kpath, &parent_path, &name);
    if (err) return err;

    int ret;
    if (flag & AT_REMOVEDIR) ret = kpath_rmdir(start, parent_path, name);
    else ret = path_unlink(start, parent_path, name);

    kfree(parent_path);
    kfree(name);
    return ret;
}
