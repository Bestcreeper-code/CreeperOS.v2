#pragma once

#include "defines/lists.h"
#include "defines/types.h"
#include "defines/compiler_defs.h"
#include "vfs.h"
#include <stdbool.h>


#define FS_INIT_FUNCS_SECTION _GCC_SECTION("fs_drivers_list") 


struct file_system_type {
    const char *name;
    struct super_block *(*mount)(struct file_system_type *, int, const char *, void *);
    void (*kill_sb)(struct super_block *);
    struct file_system_type * next;
    struct list_head fs_supers;
};
struct dentry* kpath_lookup(struct inode* start, const char* path);
int kpath_create(struct inode* start, const char* path, umode_t mode, bool excl);
int kpath_mkdir(struct inode* start, const char* path, umode_t mode);
int kpath_rmdir(struct inode* start, const char* path, char* name);
int path_unlink(struct inode* start, const char* path, char* name);

int kpath_create_force(struct inode* start, const char* path, umode_t mode, bool excl);
int kpath_mkdir_force(struct inode* start, const char* path, umode_t mode);

void tree(struct dentry* d, int depth);

