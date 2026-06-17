#pragma once

#include "defines/compiler_defs.h"

#include <stdint.h>

struct tar_header
{
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
    char padding[355];
} GCC_ATTR((packed));

int initrd_init();
unsigned int get_tar_size(const char *in);
int parse_tar(uintptr_t address);