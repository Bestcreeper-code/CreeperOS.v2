#include "mbr_partition.h"

#include "defines/err_codes.h"
#include "memory/memory.h"
#include "string/string.h"
#include "memops.h"
#include "printf/printf.h"
#include "vfs/fs.h"

// #include "printf.h"



int scan_disk_mbr_vfs(struct block_device* blkdev) {
    mbr_layout* mbr = kmalloc(sizeof(mbr_layout));
    RET_IF(!mbr, E_NOMEM);
    memset(mbr, 0, 512);
    
    if (blkdev->ops->read(blkdev, mbr, sizeof(mbr_layout), 0) != sizeof(mbr_layout)) return -E_IO;


    
#if MBR_DEBUG
    Sys_log_NoPos("mbr:{\n");
    for (int i = 0; i < 512; i++) {
        Sys_log_NoPos("%02x", ((uint8_t*)mbr)[i]);
        if ((i & 15) == 15)
            Sys_log_NoPos("\n");
    }
    Sys_log_NoPos("\n} mbr end\n");
#endif
    for(int i = 0; i<4;i++ ) {
#if MBR_DEBUG
        Sys_log("scaning /sys/devices/block/%s mbr part %d\n", blkdev->name, i);
#endif
        mbr_partition_entry* partit = &mbr->partition_table[i];

        if(partit->total_sectors == 0) continue;

#if MBR_DEBUG
        Sys_log(" /sys/devices/block/%s mbr part %d exists\n", blkdev->name, i);
#endif
        char tmp_buff[64];
        
        sprintf(tmp_buff, "/sys/devices/block/%s/%sp%d", blkdev->name, blkdev->name, i+1);
        

        kpath_create(root_dentry->inode, tmp_buff, S_IFBLK | 0660, true);

    }
}