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

int ramfile_open(struct inode* inode, struct file* file) {
    file->f_inode = inode;
    file->f_pos = 0;

    file->f_ops = &ram_file_fops;
    
    
    
    return 0;
}

ssize_t ramfile_read(struct file *file, char *buf, size_t count, loff_t *offset) {
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

ssize_t ramfile_write(struct file *file, const char *buf, size_t count, loff_t *offset)
{
    RET_IF(!file || !buf || !offset, -E_INVAL);

    struct ramfile_info *info = file->f_inode->i_private;
    RET_IF(!info || !info->start, -E_INVAL);

    size_t pos = (size_t)*offset;

    if (info->fixed_size) {
        //info->fixed_size = true;
        if (pos > info->size)
            return -E_INVAL;

        size_t available = info->size - pos;

        size_t tail_len = info->size - pos;

        size_t move_len = (tail_len > 0) ? tail_len : 0;

        if (move_len > 0) {
            size_t shift = (count < available) ? count : available;

            memmove((void*)(info->start + pos + shift),
                    (void*)(info->start + pos),
                    move_len > (available - shift) ? (available - shift) : move_len);
        }

        size_t to_write = (count < available) ? count : available;
        memcpy((void*)(info->start + pos), buf, to_write);

        *offset += to_write;
        return to_write;
    }

    //info->fixed_size = false;
    size_t new_size = info->size + count;

    void *new_mem = krealloc_impl((void*)info->start, new_size);
    if (!new_mem)
        return -E_NOMEM;

    info->start = (uintptr_t)new_mem;

    size_t tail_len = info->size - pos;
    memmove((void*)(info->start + pos + count),
            (void*)(info->start + pos),
            tail_len);

    memcpy((void*)(info->start + pos), buf, count);

    info->size = new_size;
    *offset += count;

    return count;
}

int ramfile_release(struct inode *inode, struct file *file)
{
    (void)inode;

    if (!file)
        return -E_INVAL;

    // If you ever allocate per-open state:
    // free(file->private_data);

    file->private_data = NULL;
    file->f_ops = NULL;

    return 0;
}


int ramfile_create(const char* path, uintptr_t start, size_t size,
        umode_t mode, bool fixed_size, bool excl) {
    RET_IF(!path, -E_INVAL);

    int res = kpath_create_force(root_dentry->inode, path, mode, excl);
    RET_IF(res < 0, res);

    struct dentry *dentry = kpath_lookup(root_dentry->inode, path);
    RET_IF(!dentry, -E_NOENT);
    RET_IF(!dentry->inode, -E_NOENT);

    struct ramfile_info *info = kmalloc(sizeof(struct ramfile_info));
    RET_IF(!info, -E_NOMEM);

    info->start = start;
    info->size = size;
    info->fixed_size = fixed_size;

    dentry->inode->i_private = info;
    dentry->inode->i_fop = &ram_file_fops;

    return 0;
}