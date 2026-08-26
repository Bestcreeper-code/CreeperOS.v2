#include "chardev.h"
#include "defines/err_codes.h"
#include "defines/types.h"




loff_t chardev_lseek(struct file*, loff_t, int) {
    return -E_SPIPE;
}