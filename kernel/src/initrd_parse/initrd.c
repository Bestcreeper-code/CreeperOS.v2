#include "initrd.h"
#include "debug/Logger.h"
#include "drivers/drivers.h"
#include "memory/memory.h"
#include "vfs/fs.h"
#include "vfs/ramfile.h"
#include "defines/err_codes.h"
#include "vfs/vfs.h"
#include "memops.h"

#include <stdint.h>
#include <limine.h>

/* Limine module request — place in a dedicated section so the bootloader sees it */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
};

struct tar_header *tar_file_headers[32];

int initrd_init() {
    if (!module_request.response) {
        Sys_Error("module response is null\n");
        return -E_NOENT;
    }

    struct limine_module_response *resp = module_request.response;

    struct limine_file *initrd = NULL;
    for (uint64_t i = 0; i < resp->module_count; i++) {
        struct limine_file *f = resp->modules[i];
        if (f->string && __builtin_strcmp(f->string, "initrd") == 0) {
            initrd = f;
            break;
        }
    }

    if (!initrd){ 
        Sys_Error("Initrd module not found\n");
        return -E_NOENT;
    }

    char *initrd_data = (char *)initrd->address;
    uintptr_t size    = initrd->size;

#if INITRD_DEBUG
    Sys_Debug("[initrd vaddr] %p\n", initrd_data);
    Sys_Debug("[initrd size]  %p\n", (void *)size);
#endif

    parse_tar((uintptr_t)initrd_data);

    int res = kpath_mkdir(root_dentry->inode, "/initrd", 0777);
    if (res < 0) {
        Sys_Error("Failed to create initrd directory: %d\n", res);
        return res;
    }

    struct dentry *dir_dentry = kpath_lookup(root_dentry->inode, "/initrd/");
    if (!dir_dentry) {
#if INITRD_DEBUG
        Sys_Error("Failed to lookup initrd directory\n");
#endif
        return -E_NOENT;
    }

    
    struct tar_header *header = tar_file_headers[0];
    for (int i = 0; header != NULL; header = tar_file_headers[++i]) {
        memmove(header->filename, &header->filename[2], 98);

#if INITRD_DEBUG
        Sys_Debug("File: %s, size: %u\n",
                header->filename, get_tar_size(header->size));
#endif
        struct dentry *new_dentry = kmalloc(sizeof(struct dentry));
        new_dentry->name   = header->filename;
        new_dentry->parent = dir_dentry;
        new_dentry->inode  = NULL;

        dir_dentry->inode->i_op->create(
            dir_dentry->inode,
            new_dentry,
            0777,
            true
        );

        struct ramfile_info *info = kmalloc(sizeof(struct ramfile_info));
        info->start      = (uintptr_t)initrd_data
                         + ((uintptr_t)header - (uintptr_t)initrd_data)
                         + 512;           /* skip the 512-byte TAR block header */
        info->size       = get_tar_size(header->size);
        info->fixed_size = true;

        new_dentry->inode->i_private = info;
        new_dentry->inode->i_fop    = &ram_file_fops;
    }

    return 0;
}




unsigned int get_tar_size(const char *in)
{
    unsigned int size  = 0;
    unsigned int count = 1;

    for (int j = 11; j > 0; j--, count *= 8)
        size += (unsigned int)((in[j - 1] - '0') * count);

    return size;
}

int parse_tar(uintptr_t address)
{
    int i;
    for (i = 0; ; i++) {
        struct tar_header *header = (struct tar_header *)address;

        if (header->filename[0] == '\0')
            break;

        unsigned int size   = get_tar_size(header->size);
        tar_file_headers[i] = header;

        address += ((size / 512) + 1) * 512;
        if (size % 512)
            address += 512;
    }
    return i;
}