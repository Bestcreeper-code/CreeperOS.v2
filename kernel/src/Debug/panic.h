#ifndef CRASHHNDL_H
#define CRASHHNDL_H

#include "asm/ams.h"
#include <stdint.h>

#define MAX_STACK_TRACE_SIZE 16

void panic(const char* msg);
void _panic_handler(uint64_t isr_index, uint64_t err_code, cpu_registers_t* regs, uint64_t* call_stack);
void _manual_panic(const char* error, const char* info);

#endif // CRASHHNDL_H