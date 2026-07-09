#pragma once


#include <stdint.h>
int bin_exec_raw(
    const char *path,
    uintptr_t entry_addr,
    char** argv,
    char** envp
);