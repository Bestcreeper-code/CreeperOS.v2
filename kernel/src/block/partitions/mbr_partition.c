#include "mbr_partition.h"

#include "defines/err_codes.h"
#include "memory/memory.h"
#include "string/string.h"
#include "memops.h"
#include "printf/printf.h"
#include "vfs/fs.h"
#include "vfs/ramfile.h"

// #include "printf.h"



int scan_disk_mbr_vfs(struct block_device* blkdev)
{
    RET_IF(!blkdev, -E_INVAL);

    mbr_layout* mbr = kmalloc(sizeof(mbr_layout));
    RET_IF(!mbr, -E_NOMEM);

    memset(mbr, 0, sizeof(mbr_layout));

    int ret = blkdev->ops->read(blkdev, mbr, sizeof(mbr_layout), 0);
    if (ret != sizeof(mbr_layout))
        return -E_IO;

    for (int i = 0; i < 4; i++) {

        mbr_partition_entry* part = &mbr->partition_table[i];
        if (part->total_sectors == 0)
            continue;

        char path[128];

        sprintf(path, "/sys/class/block/%s/%sp%d", blkdev->name,
                blkdev->name, i + 1);

        kpath_create_force(root_dentry->inode,
                           path,
                           S_IFBLK | 0660,
                           true);

        {
            char meta_path[160];
            char meta_buf[32];

            sprintf(meta_path,
                    "/sys/class/block/%s/%sp%d/size",
                    blkdev->name,
                    blkdev->name, i + 1);

            sprintf(meta_buf, "%u\n", part->total_sectors);

            ramfile_create(meta_path,
                           (uintptr_t)meta_buf,
                           strlen(meta_buf) + 1,
                           S_IFREG | 0444,
                           true,
                           false);
        }
    }

    return 0;
}