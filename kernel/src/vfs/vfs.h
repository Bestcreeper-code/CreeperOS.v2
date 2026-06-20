#pragma once
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>




#include "defines/lists.h"
#include "defines/types.h"





#define ROOT_UID 0


/* pathwalk mode */
#define LOOKUP_FOLLOW		BIT(0)	/* follow links at the end */
#define LOOKUP_DIRECTORY	BIT(1)	/* require a directory */
#define LOOKUP_AUTOMOUNT	BIT(2)  /* force terminal automount */
#define LOOKUP_EMPTY		BIT(3)	/* accept empty path [user_... only] */
#define LOOKUP_LINKAT_EMPTY	BIT(4) /* Linkat request with empty path. */
#define LOOKUP_DOWN		    BIT(5)	/* follow mounts in the starting point */
#define LOOKUP_MOUNTPOINT	BIT(6)	/* follow mounts in the end */
#define LOOKUP_REVAL		BIT(7)	/* tell ->d_revalidate() to trust no cache */
#define LOOKUP_RCU		    BIT(8)	/* RCU pathwalk mode; semi-internal */
#define LOOKUP_CACHED		BIT(9) /* Only do cached lookup */
#define LOOKUP_PARENT		BIT(10)	/* Looking up final parent in path */
/* 5 spare bits for pathwalk */

/* These tell filesystem methods that we are dealing with the final component... */
#define LOOKUP_OPEN		BIT(16)	/* ... in open */
#define LOOKUP_CREATE		BIT(17)	/* ... in object creation */
#define LOOKUP_EXCL		BIT(18)	/* ... in target must not exist */
#define LOOKUP_RENAME_TARGET	BIT(19)	/* ... in destination of rename() */

/* 4 spare bits for intent */

/* Scoping flags for lookup. */
#define LOOKUP_NO_SYMLINKS	BIT(24) /* No symlink crossing. */
#define LOOKUP_NO_MAGICLINKS	BIT(25) /* No nd_jump_link() crossing. */
#define LOOKUP_NO_XDEV		BIT(26) /* No mountpoint crossing. */
#define LOOKUP_BENEATH		BIT(27) /* No escaping from starting point. */
#define LOOKUP_IN_ROOT		BIT(28) /* Treat dirfd as fs root. */
/* LOOKUP_* flags which do scope-related checks based on the dirfd. */
#define LOOKUP_IS_SCOPED (LOOKUP_BENEATH | LOOKUP_IN_ROOT)


enum file_flag {
    O_RD        = 0x0,
    O_WR        = 0x1,
    O_RW        = 0X2,
    O_ACCESS    = 0x3,
    
    O_CREAT     = 0x10
};

typedef struct file {
    struct inode *f_inode;          // points to the file/directory inode
    loff_t f_pos;                   // current file position (for read/write)
    uint32_t f_flags;           // O_RDONLY, O_WRONLY, etc.
    void *private_data;             // filesystem-specific data
    const struct file_operations *f_ops; // pointer to file operations
} file;


struct file_operations {
    // struct module *owner; later waybe but commented out to remember
    int (*open) (struct inode *, struct file *);
    ssize_t (*read) (struct file *, char  *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char  *, size_t, loff_t *);
    int (*release) (struct inode *, struct file *);
};

struct inode_operations{
    struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned int);
    int (*mkdir)(struct inode *, struct dentry *, umode_t);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*create)(struct inode *, struct dentry *, umode_t, bool);
    int (*unlink)(struct inode *, struct dentry *);

    // int (*getattr)(const struct path *, struct kstat *, uint32_t, unsigned int);
    // int (*setattr)(struct dentry *, struct iattr *);

    // int (*readlink)(struct dentry *, char  *, int);
    // int (*symlink)(struct inode *, struct dentry *, const char *);
};

struct vfsmount {
    struct dentry *mnt_root;
    struct super_block *mnt_sb;
    int mnt_flags;
};

struct mount {
    struct mount *mnt_parent;
    struct dentry *mnt_mountpoint;
    struct vfsmount mnt;
    struct list_head mnt_mounts;
    struct list_head mnt_child;
};



struct inode {
    struct dentry *i_dentry;  
    // info
    umode_t    i_mode;
    kuid_t     i_uid;
    kgid_t     i_gid;
    // unsigned int i_nlink;

    // size nd metadata
    loff_t     i_size;
    time64_t   i_atime;
    time64_t   i_mtime;
    time64_t   i_ctime;
    atomic_t   i_count;
    // dev 
    dev_t      i_rdev;

    // funcs
    const struct inode_operations *i_op;
    const struct file_operations  *i_fop;

    struct super_block *i_sb;

    // type specif data
    void *i_private;
};

typedef struct dentry {
    char *name;
    struct dentry *parent;   // parent directory
    struct inode *inode;     // target (NULL = negative dentry)

    struct hlist_node d_sib;      // child of parent list
    struct hlist_head d_children;

    atomic_uint d_count;
} dentry;



extern struct inode_operations def_vfs_i_ops;
extern struct dentry* root_dentry;

struct dentry *vfs_lookup(struct inode* dir, struct dentry* file, unsigned int flags);
int vfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
int vfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

int vfs_inv_func();