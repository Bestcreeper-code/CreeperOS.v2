#pragma once

#include "scheduler/scheduler.h"
#include <stdint.h>

int exec_elf_from_vfs(tcb_t* thread, const char* vfs_path,
    char** argv, char** envp, uint64_t rsp);