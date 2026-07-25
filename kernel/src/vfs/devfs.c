#include "devfs.h"
#include "debug/Logger.h"
#include "defines/helpers.h"
#include "defines/types.h"
#include "drivers/pit/pit.h"
#include "memory/memory.h"
#include "printf/printf.h"
#include "vfs.h"
#include "defines/err_codes.h"
#include "string/string.h"
#include "memops.h"
#include "vfs/fs.h"

#include <stddef.h>
#include <stdint.h>

static uint64_t rng_state = 0;

//bogus rng just to do kind of the job
static uint64_t rng_next() {
    if (!rng_state)
        rng_state = rdtsc() ^ 0x9E3779B97F4A7C15ULL;

    /* fold in fresh timing jitter each call */
    rng_state ^= rdtsc();

    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;

    return x * 0x2545F4914F6CDD1DULL;
}

static int devfs_open(struct inode* inode, struct file* file) {
    RET_IF(!inode, -E_INVAL);

    file->f_inode = inode;
    file->f_pos = 0;

    
    file->f_ops = inode->i_fop;
    file->private_data = inode->i_private;

    return 0;
}

static int devfs_release(struct inode* inode, struct file* file) {
    (void)inode;
    RET_IF(!file, -E_INVAL);

    file->private_data = NULL;
    file->f_ops = NULL;

    return 0;
}


static ssize_t devfs_discard_write(struct file* file, const char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf, -E_INVAL);
    (void)buf;

    if (offset)
        *offset += count;

    return count;
}


static ssize_t devnull_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file, -E_INVAL);
    (void)buf;
    (void)count;
    (void)offset;
    return 0;
}

struct file_operations devnull_fops = {
    .open = devfs_open,
    .read = devnull_read,
    .write = devfs_discard_write,
    .release = devfs_release,
};




//             /dev/zero
static ssize_t devzero_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf, -E_INVAL);

    memset(buf, 0, count);

    if (offset)
        *offset += count;

    return count;
}

struct file_operations devzero_fops = {
    .open = devfs_open,
    .read = devzero_read,
    .write = devfs_discard_write,
    .release = devfs_release,
};


//   /dev/full
static ssize_t devfull_write(struct file* file, const char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf, -E_INVAL);
    (void)count;
    (void)offset;
    return -E_NOSPC;
}

struct file_operations devfull_fops = {
    .open = devfs_open,
    .read = devzero_read,
    .write = devfull_write,
    .release = devfs_release,
};


// bullshit /dev/random +s /dev/urandom

static ssize_t devrandom_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf, -E_INVAL);

    size_t written = 0;
    while (written < count) {
        uint64_t r = rng_next();
        size_t chunk = count - written;
        if (chunk > sizeof(r))
            chunk = sizeof(r);

        memcpy(buf + written, &r, chunk);
        written += chunk;
    }

    if (offset)
        *offset += written;

    return written;
}

struct file_operations devrandom_fops = {
    .open = devfs_open,
    .read = devrandom_read,
    .write = devfs_discard_write,
    .release = devfs_release,
};


//     /dev/mem
static ssize_t devmem_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf || !offset, -E_INVAL);
 
    memcpy(buf, (void*)(uintptr_t)*offset, count);
    *offset += count;
 
    return count;
}
 
static ssize_t devmem_write(struct file* file, const char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf || !offset, -E_INVAL);
 
    memcpy((void*)(uintptr_t)*offset, buf, count);
    *offset += count;
 
    return count;
}
 
struct file_operations devmem_fops = {
    .open = devfs_open,
    .read = devmem_read,
    .write = devmem_write,
    .release = devfs_release,
};
 


// /dev/port

static ssize_t devport_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf || !offset, -E_INVAL);
 
    for (size_t i = 0; i < count; i++) {
        uint16_t port = (uint16_t)(*offset + (loff_t)i);
        buf[i] = (char)inb(port);
    }
    *offset += count;
 
    return count;
}
 
static ssize_t devport_write(struct file* file, const char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf || !offset, -E_INVAL);
 
    for (size_t i = 0; i < count; i++) {
        uint16_t port = (uint16_t)(*offset + (loff_t)i);
        outb(port, (uint8_t)buf[i]);
    }
    *offset += count;
 
    return count;
}
 
struct file_operations devport_fops = {
    .open = devfs_open,
    .read = devport_read,
    .write = devport_write,
    .release = devfs_release,
};

//dev/tty
static ssize_t devtty_write(struct file* file, const char* buf, size_t count, loff_t* offset) {
    RET_IF(!file || !buf, -E_INVAL);

    ktty_write(buf, count);

    if (offset)
        *offset += count;

    return count;
}

static ssize_t devtty_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file, -E_INVAL);
    (void)buf; (void)count; (void)offset;
    return 0; 
}

struct file_operations devtty_fops = {
    .open    = devfs_open,
    .read    = devtty_read,
    .write   = devtty_write,
    .release = devfs_release,
};



int k_devfs_create(const char* path, struct file_operations* fops, umode_t mode) {
    RET_IF(!path || !fops, -E_INVAL);

    int res = kpath_create_force(root_dentry->inode, path, mode, false);
    RET_IF(res < 0, res);

    struct dentry* dentry = kpath_lookup(root_dentry->inode, path);
    RET_IF(!dentry, -E_NOENT);
    RET_IF(!dentry->inode, -E_NOENT);

    dentry->inode->i_size = 0;
    dentry->inode->i_private = NULL;
    dentry->inode->i_fop = fops;
    dentry->inode->i_op = NULL;

    return 0;
}


int devfs_init() {
    int res = kpath_mkdir(root_dentry->inode, "/dev", 0755);
    RET_IF(res < 0, res);
 
    res = k_devfs_create("/dev/null", &devnull_fops, 0666);
    RET_IF(res < 0, res);
 
    res = k_devfs_create("/dev/zero", &devzero_fops, 0666);
    RET_IF(res < 0, res);
 
    res = k_devfs_create("/dev/full", &devfull_fops, 0666);
    RET_IF(res < 0, res);
 
    res = k_devfs_create("/dev/random", &devrandom_fops, 0666);
    RET_IF(res < 0, res);
 
    res = k_devfs_create("/dev/urandom", &devrandom_fops, 0666);
    RET_IF(res < 0, res);
 
    res = k_devfs_create("/dev/mem", &devmem_fops, 0600);
    RET_IF(res < 0, res);
 
    res = k_devfs_create("/dev/port", &devport_fops, 0600);
    RET_IF(res < 0, res);

    res = k_devfs_create("/dev/tty", &devtty_fops, 0666 | S_IFCHR);
    RET_IF(res < 0, res);
 
    return 0; 
}

#include "input/input.h"

static int devfs_indev_open(struct inode* inode, struct file* file) {
    RET_IF(!inode, -E_INVAL);

    file->f_inode = inode;
    file->f_pos = 0;
    file->f_ops = inode->i_fop;
    file->private_data = inode->i_private;   // struct input_device*

    return 0;
}

static ssize_t devfs_indev_read(struct file* file, char* buf, size_t count, loff_t* offset) {
    RET_IF(!file, -E_INVAL);

    struct input_device* idev = (struct input_device*)file->private_data;
    RET_IF(!idev || !idev->ops || !idev->ops->read, -E_INVAL);

    loff_t off = offset ? *offset : 0;
    int res = idev->ops->read(idev, buf, count, off);

    if (res > 0 && offset)
        *offset += res;

    return res;
}

static ssize_t devfs_indev_write(struct file* file, const char* buf, size_t count, loff_t* offset) {
    RET_IF(!file, -E_INVAL);

    struct input_device* idev = (struct input_device*)file->private_data;
    RET_IF(!idev || !idev->ops || !idev->ops->write, -E_INVAL);

    loff_t off = offset ? *offset : 0;
    int res = idev->ops->write(idev, (void*)buf, count, off);

    if (res > 0 && offset)
        *offset += res;

    return res;
}

struct file_operations devfs_indev_fops = {
    .open    = devfs_indev_open,
    .read    = devfs_indev_read,
    .write   = devfs_indev_write,
    .release = devfs_release,
};


int devfs_register_input(struct input_device* idev) {
    RET_IF(!idev, -E_INVAL);

    int res = kpath_mkdir_force(root_dentry->inode, "/dev/input", 0755);
    RET_IF(res < 0, res);

    char path[64];
    sprintf(path, "/dev/input/input%d", idev->id);

    res = k_devfs_create(path, &devfs_indev_fops, 0640);
    RET_IF(res < 0, res);

    struct dentry* dentry = kpath_lookup(root_dentry->inode, path);
    RET_IF(!dentry, -E_NOENT);
    RET_IF(!dentry->inode, -E_NOENT);

    dentry->inode->i_private = idev;   // k_devfs_create zeroes i_private, so set it after

    return 0;
}