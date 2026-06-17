#pragma once

#include "vfs.h"
#include <stddef.h>
#include <stdint.h>
struct ramfile_info {
    uintptr_t start;
    size_t size;
    bool fixed_size;

};

extern struct file_operations ram_file_fops;

int ramfile_open(struct inode* inode, struct file* file);
ssize_t ramfile_read(struct file *file, char *buf, size_t count, loff_t *offset);
ssize_t ramfile_write(struct file *file, const char *buf, size_t count, loff_t *offset);
int ramfile_release(struct inode *inode, struct file *file);