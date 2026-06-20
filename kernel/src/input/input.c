#include "input.h"
#include "debug/Logger.h"
#include "memory/memory.h"
#include "string/string.h"
#include "defines/err_codes.h"
#include "defines/container_of.h"
#include "defines/helpers.h"
#include "vfs/sysfs.h"
#include "vfs/fs.h"
#include "config.h"

#include <stddef.h>

size_t _input_id_map[128 / (sizeof(size_t) * 8)];

struct list_head* input_device_list_start;
short input_device_amount;

struct input_device* register_input_device(const char *name, struct input_device_ops* ops, void *private_data) {
#if INPUT_DEBUG
    Sys_log("Registered input device [%s]\n", name);
#endif

    struct input_device* idev = kmalloc(sizeof(struct input_device));
    if (!idev)
        return NULL;

    idev->name = name;
    idev->ops = ops;
    idev->private_data = private_data;

    idev->id = wbitmap_alloc_1_first(
        (char*)_input_id_map,
        sizeof(_input_id_map)
    );

    if (!input_device_list_start) {
        input_device_list_start = &idev->list;
        idev->list.next = NULL;
        idev->list.prev = &idev->list;

        input_device_amount++;
        goto vfs_reg;
    }

    struct list_head* end = input_device_list_start->prev;

    RET_IF(end == 0, NULL);

    end->next = &idev->list;
    idev->list.prev = end;
    idev->list.next = NULL;

    input_device_list_start->prev = &idev->list;

    input_device_amount++;

vfs_reg:

    // sysfs registration
    int res = sysfs_register_input(idev);
    if(res != 0){
        Sys_Error("sysfs input registration failed (%d)\n",res);
        return NULL;
    }
    
    return idev;
}


int unregister_input_device(struct input_device* idev) {
    RET_IF(!idev, -E_INVAL);

    if (&idev->list == input_device_list_start) {
        input_device_list_start = idev->list.next;
        if (input_device_list_start)
            input_device_list_start->prev = idev->list.prev;
    } else {
        idev->list.prev->next = idev->list.next;

        if (!idev->list.next)
            input_device_list_start->prev = idev->list.prev;
        else
            idev->list.next->prev = idev->list.prev;
    }

    bitmap_set((char*)_input_id_map, idev->id, 0);

    kfree(idev);
    return 0;
}

int get_input_device_amount() {
    return input_device_amount;
}