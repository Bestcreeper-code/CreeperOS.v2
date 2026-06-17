#pragma once

#include "block/blkdev.h"
#include <stdint.h>

typedef struct {
    uint8_t head;
    uint16_t start_sector : 6;
    uint16_t start_cylinder : 10;
} __attribute__((packed)) head_sector_cylinder;

typedef struct {
#define MBR_BOOT_INDICATOR_ACTIVE 0x80
    uint8_t boot_indicator; // 0x80 - active, 0x00 - inactive
    
    head_sector_cylinder start_head_sec_cyl;
    
    uint8_t system_id;
    
    head_sector_cylinder end_head_sec_cyl;
    
    uint32_t relative_sector; // starting sector 
    
    uint32_t total_sectors;   // size of the partit
} __attribute__((packed)) mbr_partition_entry;






typedef struct {
    uint8_t boot_code[446];
    mbr_partition_entry partition_table[4];
    uint16_t signature; //AA55h
} __attribute__((packed)) mbr_layout;



int scan_disk_mbr_vfs(struct block_device* blkdev);