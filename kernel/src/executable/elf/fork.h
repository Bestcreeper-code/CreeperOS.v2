#pragma once

#include "scheduler/scheduler.h"

pid_t fork_process(linked_pcb* parent, uint64_t rsp);

