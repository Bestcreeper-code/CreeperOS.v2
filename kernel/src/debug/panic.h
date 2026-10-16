#ifndef CRASHHNDL_H
#define CRASHHNDL_H

#include "asm/asm.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_STACK_TRACE_SIZE 16


enum isr_type {

    ISR_DIVIDE_BY_ZERO = 0,           // 0
    ISR_DEBUG_EXCEPTION,              // 1
    ISR_NMI,                          // 2 Non-Maskable Interrupt
    ISR_BREAKPOINT,                   // 3
    ISR_OVERFLOW,                     // 4
    ISR_BOUND_RANGE_EXCEEDED,         // 5
    ISR_INVALID_OPCODE,               // 6
    ISR_DEVICE_NOT_AVAILABLE,         // 7
    ISR_DOUBLE_FAULT,                 // 8
    ISR_COPROCESSOR_SEGMENT_OVERRUN, // 9 Obsolete
    ISR_INVALID_TSS,                  // 10
    ISR_SEGMENT_NOT_PRESENT,          // 11
    ISR_STACK_SEGMENT_FAULT,          // 12
    ISR_GENERAL_PROTECTION,           // 13
    ISR_PAGE_FAULT,                   // 14
    ISR_RESERVED_15,                  // 15


    ISR_X87_FPU_EXCEPTION,            // 16
    ISR_ALIGNMENT_CHECK,              // 17
    ISR_MACHINE_CHECK,                // 18
    ISR_SIMD_FP_EXCEPTION,            // 19
    ISR_VIRTUALIZATION_EXCEPTION,     // 20
    ISR_CONTROL_PROTECTION_EXCEPTION, // 21


    ISR_RESERVED_22,                  // 22
    ISR_RESERVED_23,                  // 23
    ISR_RESERVED_24,                  // 24
    ISR_RESERVED_25,                  // 25
    ISR_RESERVED_26,                  // 26
    ISR_RESERVED_27,                  // 27


    ISR_HYPERVISOR_INJECTION,         // 28
    ISR_VMM_COMMUNICATION,            // 29


    ISR_SECURITY_EXCEPTION,           // 30
    ISR_RESERVED_31,                  // 31

    ISR_CODES_AMOUNT                  // total count
};



typedef enum { TRAP_HANDLED, TRAP_UNHANDLED_PANIC } isr_ret;
typedef isr_ret (*isr_handler)(cpu_registers_t* regs, size_t* call_stack);



void panic(const char* msg);
void _isr_handler(size_t isr_index, size_t err_code, cpu_registers_t* regs, size_t* call_stack);
void _panic_dump(const char* error, const char* info,
    uint64_t isr_index, uint64_t err_code,
    cpu_registers_t* regs, uint64_t* call_stack);


isr_ret handle_double_fault(cpu_registers_t* regs, size_t* call_stack);
isr_ret handle_page_fault(cpu_registers_t* regs, size_t* call_stack);
#endif // CRASHHNDL_H