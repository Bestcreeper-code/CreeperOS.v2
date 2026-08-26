#pragma once

#include "scheduler/scheduler.h"

pid_t fork_process(tcb_t* parent_thread, uint64_t rsp);

