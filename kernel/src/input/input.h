#pragma once

#include "defines/lists.h"
#include "defines/types.h"
#include <stddef.h>

struct input_device;

struct input_device_ops {
    int (*read)(struct input_device*, void *buf, size_t size, loff_t offset);
    int (*write)(struct input_device*, const void *buf, size_t size, loff_t offset);
    int (*ioctl)(struct input_device*, int op, ...);
};

typedef struct input_device {
    int id;
    const char *name;
    void *private_data;

    struct input_device_ops* ops;

    struct list_head list;
} input_device;


struct input_device* register_input_device(const char *name, 
        struct input_device_ops* ops, void *private_data);
int unregister_input_device(struct input_device* idev);
int Get_Input_Device_Amount();