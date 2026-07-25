#include "initrd.h"
#include "debug/Logger.h"
#include "drivers/drivers.h"
#include "memory/memory.h"
#include "requests.h"
#include "vfs/fs.h"
#include "vfs/ramfile.h"
#include "defines/err_codes.h"
#include "vfs/vfs.h"
#include "memops.h"
#include "string/string.h"

#include <stdint.h>
#include <stddef.h>
#include <limine.h>


static struct tar_header **tar_file_headers = NULL;
static int tar_file_count = 0;

unsigned int get_tar_size(const char *in)
{
    unsigned int size  = 0;
    unsigned int count = 1;

    for (int j = 11; j > 0; j--, count *= 8)
        size += (unsigned int)((in[j - 1] - '0') * count);

    return size;
}

static int count_tar_entries(uintptr_t address)
{
    int count = 0;
    for (;;) {
        struct tar_header *header = (struct tar_header *)address;

        if (header->filename[0] == '\0')
            break;

        unsigned int size = get_tar_size(header->size);

        count++;
        address += 512;
        address += ((size + 511) / 512) * 512;
    }
    return count;
}

int parse_tar(uintptr_t address)
{
    int total = count_tar_entries(address);
    if (total <= 0) {
        tar_file_count = 0;
        return 0;
    }

    tar_file_headers = kmalloc(sizeof(struct tar_header *) * (total + 1));
    if (!tar_file_headers) {
        Sys_Error("Failed to allocate tar header table\n");
        return -E_NOMEM;
    }

    int i;
    for (i = 0; i < total; i++) {
        struct tar_header *header = (struct tar_header *)address;

        if (header->filename[0] == '\0')
            break;

        unsigned int size   = get_tar_size(header->size);
        tar_file_headers[i] = header;

        address += 512;
        address += ((size + 511) / 512) * 512;
    }

    tar_file_headers[i] = NULL;
    tar_file_count = i;

    return i;
}

static char* normalize_tar_path(char *name)
{
    if (name[0] == '.' && name[1] == '/')
        memmove(name, name + 2, strlen(name + 2) + 1);

    while (name[0] == '/')
        memmove(name, name + 1, strlen(name + 1) + 1);

    return name;
}

static struct dentry *ensure_parent_dirs(struct dentry *base, const char *path)
{
    const char *slash = __builtin_strrchr(path, '/');
    if (!slash)
        return base;

    size_t dir_len = (size_t)(slash - path);
    char dirpath[256];
    if (dir_len >= sizeof(dirpath))
        dir_len = sizeof(dirpath) - 1;

    memcpy(dirpath, path, dir_len);
    dirpath[dir_len] = '\0';

    struct dentry *dir = base;
    char component[256];
    size_t start = 0;
    size_t len = strlen(dirpath);

    for (size_t i = 0; i <= len; i++) {
        if (dirpath[i] != '/' && dirpath[i] != '\0')
            continue;

        size_t clen = i - start;
        if (clen == 0) {
            start = i + 1;
            continue;
        }
        if (clen >= sizeof(component))
            clen = sizeof(component) - 1;

        memcpy(component, dirpath + start, clen);
        component[clen] = '\0';

        int res = kpath_mkdir(dir->inode, component, 0777);
        if (res < 0 && res != -E_EXIST) {
            Sys_Error("Failed to create initrd subdir '%s': %d\n", component, res);
            return NULL;
        }

        struct dentry *next = kpath_lookup(dir->inode, component);
        if (!next) {
            Sys_Error("Failed to lookup initrd subdir '%s'\n", component);
            return NULL;
        }

        dir = next;
        start = i + 1;
    }

    return dir;
}

int initrd_init() {
    if (!limine_modules_req.response) {
        Sys_Error("module response is null\n");
        return -E_NOENT;
    }

    struct limine_module_response *resp = limine_modules_req.response;

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

    int nheaders = parse_tar((uintptr_t)initrd_data);
    if (nheaders < 0)
        return nheaders;

#if INITRD_DEBUG
    Sys_Debug("[initrd] %d entries found\n", nheaders);
#endif

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

    for (int i = 0; i < nheaders; i++) {
        struct tar_header *header = tar_file_headers[i];
        if (!header)
            break;

        char *path = normalize_tar_path(header->filename);
        size_t plen = strlen(path);

        if (plen == 0 || path[plen - 1] == '/')
            continue;

#if INITRD_DEBUG
        Sys_Debug("File: %s, size: %u\n", path, get_tar_size(header->size));
#endif

        struct dentry *parent_dentry = ensure_parent_dirs(dir_dentry, path);
        if (!parent_dentry) {
            Sys_Error("Skipping '%s': could not create parent directories\n", path);
            continue;
        }

        const char *slash = __builtin_strrchr(path, '/');
        const char *leaf   = slash ? slash + 1 : path;

        struct dentry *new_dentry = kmalloc(sizeof(struct dentry));
        new_dentry->name   = (char *)leaf;
        new_dentry->parent = parent_dentry;
        new_dentry->inode  = NULL;

        parent_dentry->inode->i_op->create(
            parent_dentry->inode,
            new_dentry,
            0777,
            true
        );

        struct ramfile_info *info = kmalloc(sizeof(struct ramfile_info));
        info->start = (uintptr_t)initrd_data
                         + ((uintptr_t)header - (uintptr_t)initrd_data)
                         + 512;
        info->size = get_tar_size(header->size);
        info->fixed_size = true;

        new_dentry->inode->i_size = info->size;
        new_dentry->inode->i_private = info;
        new_dentry->inode->i_fop = &ram_file_fops;
    }

    return 0;
}