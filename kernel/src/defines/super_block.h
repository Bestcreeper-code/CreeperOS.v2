#pragma once
#include "vfs.h"
#include "path.h"


struct super_block;







struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *inode);
    void (*free_inode)(struct inode *inode);
    int (*write_inode)(struct inode *inode);
    void (*evict_inode)(struct inode *inode);
    void (*put_super)(struct super_block *sb);
    // int (*statfs)(struct dentry *dentry, struct kstatfs *kstatfs);
};

struct super_block {
    dev_t s_dev;                      // device id
    unsigned char s_blocksize_bits;
    unsigned long s_blocksize;
    loff_t s_maxbytes;                // max file size
    struct file_system_type *s_type;  // filesystem type
    const struct super_operations *s_op; // FS operations
    struct dentry *s_root;            // root dentry
    unsigned long s_flags;            // read-only, etc.
    void *s_fs_info;                  // FS-private data
    struct list_head s_inodes;        // all inodes
};