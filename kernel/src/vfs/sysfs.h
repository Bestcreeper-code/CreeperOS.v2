#pragma once

#include "block/blkdev.h"
#include "defines/types.h"
#include "input/input.h"
#include "vfs.h"




int sysfs_init();

int sysfs_register_block(struct block_device* blkdev);
int sysfs_register_input(struct input_device* idev);