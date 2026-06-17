#pragma once

#include "defines/lists.h"
#include "defines/types.h"
#include <stddef.h>

#include "defines/ioctl.h"




#define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size /512 (long *arg) */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKRASET   _IO(0x12,98)	/* set read ahead for block device */
#define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */
#define BLKFRASET  _IO(0x12,100)/* set filesystem (mm/filemap.c) read-ahead */
#define BLKFRAGET  _IO(0x12,101)/* get filesystem (mm/filemap.c) read-ahead */
#define BLKSECTSET _IO(0x12,102)/* set max sectors per request (ll_rw_blk.c) */
#define BLKSECTGET _IO(0x12,103)/* get max sectors per request (ll_rw_blk.c) */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */

struct block_device;

struct block_device_ops {
    int (*read)(struct block_device *, void *buf, size_t data_size, loff_t read_addr);
    int (*write)(struct block_device *, const void *buf, size_t data_size, loff_t write_addr);
    int (*ioctl)(struct block_device *,int op,...);
};

struct block_device {
    int id;
    const char *name;
    void *private_data;

    lsize_t size;
    size_t block_size;
    
    struct block_device_ops* ops;

    struct list_head list;
};

struct block_device* Register_Block_Device(const char *name, lsize_t size,
    size_t block_size, struct block_device_ops* ops, void *private_data);

int Unregister_Block_Device(struct block_device* blkdev);

const struct block_device* Get_Block_Device(int id);

int Get_Block_Device_Amount();
