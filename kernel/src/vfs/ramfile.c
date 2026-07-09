#include "vfs/ramfile.h"
#include "defines/helpers.h"
#include "memory/memory.h"
#include "vfs.h"
#include "defines/err_codes.h"
#include "string/string.h"
#include "memops.h"
#include "vfs/fs.h"

#include <stddef.h>
#include <stdint.h>

struct file_operations ram_file_fops= {
    .open = ramfile_open,
    .read = ramfile_read,
    .write = ramfile_write,
    .release = ramfile_release,
};

struct inode_operations ram_dir_iops = {
    .lookup = vfs_lookup,

    .create = ramfile_inode_create,
    .mkdir  = ramfile_inode_mkdir,
    .rmdir  = ramfile_inode_rmdir,
    .unlink = ramfile_inode_unlink,
};





int ramfile_open(struct inode* inode, struct file* file) {
    RET_IF(!inode, -E_INVAL);
    
    file->f_inode = inode;
    file->f_pos = 0;

    file->f_ops = &ram_file_fops;

    file->private_data = inode->i_private;

    return 0;
}

ssize_t ramfile_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file, -E_INVAL);

    struct ramfile_info* info = file->f_inode->i_private;

    RET_IF(!info->start, -E_INVAL);
    RET_IF(!info->size, 0);

    size_t bytes_left = info->size - *offset;
    size_t bytes_to_read = (count < bytes_left) ? count : bytes_left;

    memcpy(buf, (void*)(info->start + (uintptr_t)*offset), bytes_to_read);
    *offset += bytes_to_read;

    return bytes_to_read;
}

ssize_t ramfile_write(struct file* file, const char* buf, size_t count, loff_t* offset)
{
    RET_IF(!file || !buf || !offset, -E_INVAL);

    struct ramfile_info* info = file->f_inode->i_private;
    RET_IF(!info, -E_INVAL);

    size_t pos = (size_t)*offset;

    if (info->fixed_size) {
        RET_IF(!info->start, -E_INVAL);
        if (pos > info->size)
            return -E_INVAL;

        size_t available = info->size - pos;
        size_t to_write   = (count < available) ? count : available;

        memcpy((void*)(info->start + pos), buf, to_write);

        *offset += to_write;
        return to_write;
    }

    if (count == 0) {
        *offset += 0;
        return 0;
    }

    if (pos + count < pos)
        return -E_INVAL;

    size_t new_end = pos + count;

    if (new_end <= info->size) {
        memcpy((void*)(info->start + pos), buf, count);
    } else {
        void* new_mem = krealloc_impl((void*)info->start, new_end);
        if (!new_mem)
            return -E_NOMEM;

        info->start = (uintptr_t)new_mem;

        if (pos > info->size) {
            memset((void*)(info->start + info->size), 0, pos - info->size);
        }

        memcpy((void*)(info->start + pos), buf, count);

        info->size = new_end;
        file->f_inode->i_size = new_end;
    }

    *offset += count;
    return count;
}

int ramfile_release(struct inode* inode, struct file* file)
{
    (void)inode;

    if (!file)
        return -E_INVAL;

    file->private_data = NULL;
    file->f_ops = NULL;
 
    return 0;
}

int k_ramfile_create(const char* path, uintptr_t start, size_t size,
        umode_t mode, bool fixed_size, bool excl) {
    RET_IF(!path, -E_INVAL);

    int res = kpath_create_force(root_dentry->inode, path, mode, excl);
    RET_IF(res < 0, res);

    struct dentry* dentry = kpath_lookup(root_dentry->inode, path);
    RET_IF(!dentry, -E_NOENT);
    RET_IF(!dentry->inode, -E_NOENT);

    struct ramfile_info* info = kmalloc(sizeof(struct ramfile_info));
    RET_IF(!info, -E_NOMEM);

    info->start = start;
    info->size = size;
    info->fixed_size = fixed_size;

    dentry->inode->i_size = size;
    dentry->inode->i_private = info;
    dentry->inode->i_fop = &ram_file_fops;
    dentry->inode->i_op = &ram_dir_iops;

    return 0;
}

int k_ramfile_mkdir(const char* path, umode_t mode) {
    RET_IF(!path, -E_INVAL);

    int res = kpath_mkdir(root_dentry->inode, path, mode);
    RET_IF(res < 0, res);

    struct dentry* dentry = kpath_lookup(root_dentry->inode, path);
    RET_IF(!dentry, -E_NOENT);
    RET_IF(!dentry->inode, -E_NOENT);

    dentry->inode->i_fop = &ram_file_fops;
    dentry->inode->i_op = &ram_dir_iops;

    return 0;
}





int ramfile_inode_create(struct inode* dir, struct dentry* dentry, umode_t mode, bool excl) {
    RET_IF(!dir || !dentry, -E_INVAL);

    if (excl && dentry->inode)
        return -E_EXIST;

    int res = vfs_create(dir, dentry, S_IFREG | (mode & ~S_IFMT), excl);
    RET_IF(res < 0, res);

    struct ramfile_info* info = kmalloc(sizeof(struct ramfile_info));
    if (!info) {
        kfree(dentry->inode);
        dentry->inode = NULL;
        return -E_NOMEM;
    }

    info->start = 0;
    info->size = 0;
    info->fixed_size = false;

    dentry->inode->i_private = info;
    dentry->inode->i_fop = &ram_file_fops;
    dentry->inode->i_op = &ram_dir_iops;

    return 0;
}

int ramfile_inode_mkdir(struct inode* dir, struct dentry* dentry, umode_t mode) {
    RET_IF(!dir || !dentry, -E_INVAL);

    int res = vfs_create(dir, dentry, S_IFDIR | (mode & ~S_IFMT), false);
    RET_IF(res < 0, res);

    dentry->inode->i_op = &ram_dir_iops;
    dentry->inode->i_fop = NULL;
    dentry->inode->i_private = NULL;

    return 0;
}

int ramfile_inode_rmdir(struct inode* dir, struct dentry* dentry) {
    RET_IF(!dir || !dentry, -E_INVAL);

    struct dentry* target = vfs_lookup(dir, dentry, 0);
    RET_IF(!target, -E_NOENT);
    RET_IF(!S_ISDIR(target->inode->i_mode), -E_NOTDIR);
    RET_IF(target->d_children.first, -E_NOTEMPTY);

    hlist_del(&target->d_sib);

    kfree(target->inode->i_private);
    kfree(target->inode);
    kfree(target->name);
    kfree(target);

    return 0;
}

int ramfile_inode_unlink(struct inode* dir, struct dentry* dentry) {
    RET_IF(!dir || !dentry, -E_INVAL);

    struct dentry* target = vfs_lookup(dir, dentry, 0);
    RET_IF(!target, -E_NOENT);
    RET_IF(S_ISDIR(target->inode->i_mode), -E_ISDIR);

    hlist_del(&target->d_sib);

    struct ramfile_info* info = target->inode->i_private;
    if (info) {
        if (!info->fixed_size && info->start)
            kfree((void*)info->start);
        kfree(info);
    }

    if (--target->inode->i_count <= 0)
        kfree(target->inode);

    kfree(target->name);
    kfree(target);

    return 0;
}