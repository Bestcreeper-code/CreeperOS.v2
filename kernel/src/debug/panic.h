#ifndef CRASHHNDL_H
#define CRASHHNDL_H

#include "asm/asm.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_STACK_TRACE_SIZE 16

void panic(const char* msg);
void _panic_handler(size_t isr_index, size_t err_code, cpu_registers_t* regs, size_t* call_stack);
void _manual_panic(const char* error, const char* info);

#endif // CRASHHNDL_H