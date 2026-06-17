#include "fs.h"





int sysfs_init(){
    kpath_mkdir(root_dentry->inode, "/sys", 0555);
    kpath_mkdir(root_dentry->inode, "/sys/devices", 0755);    
}