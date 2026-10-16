#include "debug/panic.h"


#include "arch/vmm.h"
#include "debug/Logger.h"
#include "defines/signals.h"
#include "printf/printf.h"
#include "scheduler/scheduler.h"
#include "cpu/gdt.h"
#include "timer/time.h"
#include "asm/asm.h"
#include "config.h"
#include "defines/signals.h"
#include <stddef.h>
#include <stdint.h>




static isr_handler isr_handlers[32] = {
    [ISR_DOUBLE_FAULT] = handle_double_fault,
    [ISR_PAGE_FAULT] = handle_page_fault
};

// CPU Exceptions
static const char* isr_messages[] = {
    "Divide by Zero",                    // 0
    "Debug Exception",                   // 1
    "NMI",                               // 2
    "Breakpoint",                        // 3
    "Overflow",                          // 4
    "Bound Range Exceeded",              // 5
    "Invalid Opcode",                    // 6
    "Device Not Available",              // 7
    "Double Fault",                      // 8
    "Coprocessor Segment Overrun",       // 9
    "Invalid TSS",                       // 10
    "Segment Not Present",               // 11
    "Stack Segment Fault",               // 12
    "General Protection Fault",          // 13
    "Page Fault",                        // 14
    "Reserved",                          // 15


    "x87 Floating-Point Exception",      // 16
    "Alignment Check",                   // 17
    "Machine Check",                     // 18
    "SIMD Floating-Point Exception",     // 19
    "Virtualization Exception",          // 20
    "Control Protection Exception",      // 21


    "Reserved",                          // 22
    "Reserved",                          // 23
    "Reserved",                          // 24
    "Reserved",                          // 25
    "Reserved",                          // 26
    "Reserved",                          // 27


    "Hypervisor Injection Exception",    // 28
    "VMM Communication Exception",       // 29


    "Security Exception",                // 30
    "Reserved"                           // 31
};



static const char* isr_error_bits[ISR_CODES_AMOUNT][32] = {

    // 13 - General Protection Fault
    [ISR_GENERAL_PROTECTION] = {
        "External event (EXT)",            // 0
        "Descriptor location (IDT=1)",     // 1
        "Table indicator (LDT=1)",         // 2
        "Selector index bit 0",            // 3
        "Selector index bit 1",            // 4
        "Selector index bit 2",            // 5
        "Selector index bit 3",            // 6
        "Selector index bit 4",            // 7
        "Selector index bit 5",            // 8
        "Selector index bit 6",            // 9
        "Selector index bit 7",            // 10
        "Selector index bit 8",            // 11
        "Selector index bit 9",            // 12
        "Selector index bit 10",           // 13
        "Selector index bit 11",           // 14
        "Selector index bit 12",           // 15
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0
    },

    // 14 - Page Fault
    [ISR_PAGE_FAULT] = {
        "Present",  // 0
        "Write access",                          // 1
        "User mode access",                      // 2
        "Reserved bit violation",                // 3
        "Instruction fetch",                     // 4
        "Protection key violation",              // 5
        "Shadow stack access",                   // 6
        "HLAT violation",                        // 7
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0
    }
};

void log_isr_error_code(unsigned char isr_idx, uint64_t code){
    const char** errcodes_array = isr_error_bits[isr_idx];

    for(int i=0;i < 31;i++){
        if(errcodes_array[i] ){
            bool set = (code & 1 << i);
            Sys_log_NoPos("%s%s: %s\n" ESC_RESET,set? ESC_GREEN:ESC_RED, errcodes_array[i], set? "Yes":"No");
        }
    }
}





volatile char panic_count = 0;

void _isr_handler(uint64_t isr_index, uint64_t err_code, cpu_registers_t* regs, uint64_t* call_stack) {
    cli();
    klogger_is_threaded = false;
    panic_count++;

    if (isr_index < 32 && isr_handlers[isr_index]) {
        isr_ret ret = isr_handlers[isr_index](regs, call_stack);
        if (ret == TRAP_HANDLED) return;
    }
    
    const char* error_name =
        (isr_index < (sizeof(isr_messages) / sizeof(isr_messages[0])))
        ? isr_messages[isr_index]
        : "Unknown Crash";

    _panic_dump(error_name, NULL, isr_index, err_code, regs, call_stack);
}


void panic(const char* msg) {
    _panic_dump(msg, NULL, 0, 0, NULL, NULL);
}

void _panic_dump(const char* error, const char* info,
                uint64_t isr_index, uint64_t err_code,
                cpu_registers_t* regs, uint64_t* call_stack) {
    klogger_is_threaded = false;

    if (regs) {
        Sys_log("Kernel panic (%lu | %#lx | CR2:%#lx)\n",
                (uint64_t)isr_index,
                (uint64_t)err_code,
                (uint64_t)regs->cr2
        );

        Sys_log_NoPos(ESC_RED);
        
        if (error) {
            Sys_Fatal("Error: %s\n", error);
        }
        if (info) {
            Sys_log("Info : %s\n", info);
        }

        Sys_log_NoPos("=======================================================================\n");
        Sys_log_NoPos("PANIK -> ISR Index: %lu (%s), Error Code: %#lx\n",
                      isr_index, error, err_code);

        Sys_log_NoPos(" A critical error has occurred");
        if (current_process && current_process->name)
            Sys_log_NoPos(" in process %s (pid: %d)", current_process->name, current_process->pid);
        Sys_log_NoPos(" in thread %d:\n", current_thread->tid);

        Sys_log_NoPos(" Error Code: %s (%03ld: %lx)\n",
                      error, isr_index, err_code);

        Sys_log_NoPos("Error Bits:\n");
        log_isr_error_code(isr_index, err_code);

        Sys_log_NoPos(" Regs Dump:\n");

        Sys_log_NoPos(" rax: 0x%lx  rbx: 0x%lx  rcx: 0x%lx  rdx: 0x%lx\n",
                      regs->rax, regs->rbx, regs->rcx, regs->rdx);

        Sys_log_NoPos(" rsi: 0x%lx  rdi: 0x%lx  rbp: 0x%lx  rsp: 0x%lx\n",
                      regs->rsi, regs->rdi, regs->rbp, regs->rsp);
        Sys_log_NoPos(" RIP: 0x%lx  RFLAGS: 0x%lx\n",
                      regs->rip, regs->rflags);
        Sys_log_NoPos(" r8: 0x%lx  r9: 0x%lx  r10: 0x%lx  r11: 0x%lx\n",
                      regs->r8, regs->r9, regs->r10, regs->r11);

        Sys_log_NoPos(" r12: 0x%lx  r13: 0x%lx  r14: 0x%lx  r15: 0x%lx\n",
                      regs->r12, regs->r13, regs->r14, regs->r15);

        Sys_log_NoPos(" CS:  0x%04X  DS:  0x%04X  ES:  0x%04X\n",
                      regs->cs, regs->ds, regs->es);

        Sys_log_NoPos(" FS:  0x%04X  GS:  0x%04X  SS:  0x%04X\n",
                      regs->fs, regs->gs, regs->ss);

        Sys_log_NoPos(" CR0: 0x%016lx  \e[31mCR2: 0x%016lx\e[0m  CR3: 0x%016lx\n",
                      regs->cr0, regs->cr2, regs->cr3);
        Sys_log_NoPos(" CR4: 0x%016lx  CR8: 0x%016lx\n",
                      regs->cr4, regs->cr8);

        sti();
        void dump_userspace_mappings(uint64_t* pml4);
        dump_userspace_mappings(PHYS_2_HHDM(regs->cr3));
        if (call_stack) {
            Sys_log_NoPos(" Call Stack Trace:\n");
            for (int i = 0; i < MAX_STACK_TRACE_SIZE; i++) {
                uintptr_t addr = call_stack[i];
                if (addr == 0) {
                    break;
                }
                Sys_log_NoPos("    %s (%p)\n", "???", (void*)addr);
            }
        }
    } else {
        Sys_Fatal("kernel panic triggered!\n");
        Sys_Fatal("  Error: %s\n", error ? error : "(null)");
        Sys_Fatal("  Info : %s\n", info ? info : "(null)");
    }

    Sys_log("Halting...");
    hcf();
}
    


isr_ret handle_double_fault(cpu_registers_t* regs, size_t* call_stack) {
    Sys_Fatal("Double Fault (%#lx)\n", regs->cr2);
    Sys_Info("Fix your shit\n");
    hcf();
}

isr_ret handle_zero_div(cpu_registers_t* regs, size_t* call_stack) {
    kill_current_process(regs, 128 + SIGFPE, "divide by zero");
}


isr_ret handle_page_fault(cpu_registers_t* regs, size_t* call_stack) {
    bool null_deref = regs->cr2 < PAGE_SIZE_4K;
    kill_current_process(regs, 128 + SIGSEGV,
        null_deref ? "null pointer dereference" : "page fault");
}