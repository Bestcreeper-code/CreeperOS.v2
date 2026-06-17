#pragma once

#include "blkdev.h"
#include "types.h"
struct disk_partition{
    struct block_device* blkdev;
    
    lba64 start_lba;
    lsize_t size;

};