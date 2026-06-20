#include "block/blkdev.h"
#include "defines/err_codes.h"
#include "fs.h"
#include "input/input.h"
#include "memory/memory.h"
#include "printf/printf.h"
#include "vfs/ramfile.h"
#include "string/string.h"





int sysfs_init(){
    kpath_mkdir(root_dentry->inode, "/sys", 0555);
    kpath_mkdir(root_dentry->inode, "/sys/devices", 0655);    
    kpath_mkdir(root_dentry->inode, "/sys/class", 0655);    
    kpath_mkdir(root_dentry->inode, "/sys/class/block", 0655);    
    kpath_mkdir(root_dentry->inode, "/sys/class/input", 0655);    
}


int sysfs_register_block(struct block_device* blkdev) {
    RET_IF(!blkdev, -E_INVAL);

    char dir[64];
    sprintf(dir, "/sys/class/block/%d", blkdev->name);

    int res = kpath_mkdir(root_dentry->inode, dir, 0555);
    RET_IF(res < 0, res);

    //name
    
    char path[96];
    sprintf(path, "%s/name", dir);

    size_t len = strlen(blkdev->name) + 2;
    char* buf = kmalloc(len);
    RET_IF(!buf, -E_NOMEM);

    sprintf(buf, "%s\n", blkdev->name);

    ramfile_create(path, (uintptr_t)buf, len, S_IFREG | 0444, true, false);


    //size
    
    sprintf(path, "%s/size", dir);

    buf = kmalloc(32);
    RET_IF(!buf, -E_NOMEM);

    len = sprintf(buf, "%llu\n", blkdev->size);

    ramfile_create(path, (uintptr_t)buf, len, S_IFREG | 0444, true, false);
    

    //block_size
    
    sprintf(path, "%s/block_size", dir);

    buf = kmalloc(32);
    RET_IF(!buf, -E_NOMEM);

    len = sprintf(buf, "%zu\n", blkdev->block_size);

    ramfile_create(path, (uintptr_t)buf, len, S_IFREG | 0444, true, false);


    return 0;
}



int sysfs_register_input(struct input_device* idev) {
    RET_IF(!idev, -E_INVAL);

    char dir[64];
    sprintf(dir, "/sys/class/input/input%d", idev->id);

    int res = kpath_mkdir(root_dentry->inode, dir, 0555);
    RET_IF(res < 0, res);

    char path[96];
    char* buf;
    size_t len;

    //name
    sprintf(path, "%s/name", dir);

    len = strlen(idev->name) + 2;
    buf = kmalloc(len);
    RET_IF(!buf, -E_NOMEM);

    sprintf(buf, "%s\n", idev->name);

    ramfile_create(path, (uintptr_t)buf, len, S_IFREG | 0444, true, false);

    //id
    sprintf(path, "%s/id", dir);

    buf = kmalloc(32);
    RET_IF(!buf, -E_NOMEM);

    len = sprintf(buf, "%d\n", idev->id);

    ramfile_create(path, (uintptr_t)buf, len, S_IFREG | 0444, true, false);

    return 0;
}