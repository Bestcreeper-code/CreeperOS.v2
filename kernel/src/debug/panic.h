#ifndef CRASHHNDL_H
#define CRASHHNDL_H

#include "asm/asm.h"
#include <stddef.h>
#include <stdint.h>

#define DISABLE_ISR_BACKTRACE 1

#define MAX_STACK_TRACE_SIZE 16

void panic(const char* msg);
void _panic_handler(size_t isr_index, size_t err_code, cpu_registers_t* regs, size_t* call_stack);
void _manual_panic(const char* error, const char* info);

void handle_page_fault(cpu_registers_t* regs, size_t* call_stack);
#endif // CRASHHNDL_H