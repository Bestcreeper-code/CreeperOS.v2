#pragma once

#include "defines/types.h"
#include "vfs/vfs.h"

loff_t chardev_lseek(struct file*, loff_t, int);