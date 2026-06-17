#include "panic.h"

#include "Debug/Logger.h"
#include "timer/time.h"
#include "asm/ams.h"
#include "memops.h"
#include "printf/printf.h"
#include <stddef.h>
#include <stdint.h>




#define MAX_KPANIK_COUNT 1

// CPU Exceptions
static const char* crash_messages[] = {
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

    // x87 / SIMD / CPU-specific
    "x87 Floating-Point Exception",      // 16
    "Alignment Check",                   // 17
    "Machine Check",                     // 18
    "SIMD Floating-Point Exception",     // 19
    "Virtualization Exception",          // 20
    "Control Protection Exception",      // 21

    // Reserved / Unknown
    "Reserved",                          // 22
    "Reserved",                          // 23
    "Reserved",                          // 24
    "Reserved",                          // 25
    "Reserved",                          // 26
    "Reserved",                          // 27

    // Hypervisor
    "Hypervisor Injection Exception",    // 28
    "VMM Communication Exception",       // 29

    // Security / Other
    "Security Exception",                // 30
    "Reserved"                           // 31
};


enum CrashType {
    // CPU Exceptions
    CRASH_DIVIDE_BY_ZERO = 0,           // 0
    CRASH_DEBUG_EXCEPTION,              // 1
    CRASH_NMI,                          // 2 Non-Maskable Interrupt
    CRASH_BREAKPOINT,                   // 3
    CRASH_OVERFLOW,                     // 4
    CRASH_BOUND_RANGE_EXCEEDED,         // 5
    CRASH_INVALID_OPCODE,               // 6
    CRASH_DEVICE_NOT_AVAILABLE,         // 7
    CRASH_DOUBLE_FAULT,                 // 8
    CRASH_COPROCESSOR_SEGMENT_OVERRUN, // 9 Obsolete
    CRASH_INVALID_TSS,                  // 10
    CRASH_SEGMENT_NOT_PRESENT,          // 11
    CRASH_STACK_SEGMENT_FAULT,          // 12
    CRASH_GENERAL_PROTECTION,           // 13
    CRASH_PAGE_FAULT,                   // 14
    CRASH_RESERVED_15,                  // 15

    // x87 / SIMD / CPU-specific Exceptions
    CRASH_X87_FPU_EXCEPTION,            // 16
    CRASH_ALIGNMENT_CHECK,              // 17
    CRASH_MACHINE_CHECK,                // 18
    CRASH_SIMD_FP_EXCEPTION,            // 19
    CRASH_VIRTUALIZATION_EXCEPTION,     // 20
    CRASH_CONTROL_PROTECTION_EXCEPTION, // 21

    // Reserved / Unknown
    CRASH_RESERVED_22,                  // 22
    CRASH_RESERVED_23,                  // 23
    CRASH_RESERVED_24,                  // 24
    CRASH_RESERVED_25,                  // 25
    CRASH_RESERVED_26,                  // 26
    CRASH_RESERVED_27,                  // 27

    // Hypervisor
    CRASH_HYPERVISOR_INJECTION,         // 28
    CRASH_VMM_COMMUNICATION,            // 29

    // Security / Other
    CRASH_SECURITY_EXCEPTION,           // 30
    CRASH_RESERVED_31,                  // 31

    CRASH_CODES_AMOUNT                  // total count
};

char* isr_error_bits[CRASH_CODES_AMOUNT][32] = {

    // 13 - General Protection Fault
    [CRASH_GENERAL_PROTECTION] = {
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
    [CRASH_PAGE_FAULT] = {
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

void _Log_Isr_Error_Code(unsigned char isr_idx, uint64_t code){
    char** errcodes_array = isr_error_bits[isr_idx];

    for(int i=0;i < 31;i++){
        if(errcodes_array[i] ){
            bool set = (code & 1 << i);
            Sys_log_NoPos("%s%s: %s\n" ESC_RESET,set? ESC_GREEN:ESC_RED, errcodes_array[i], set? "Yes":"No");
        }
    }
}





volatile char panic_count = 0;
void _panic_handler(uint64_t isr_index, uint64_t err_code, cpu_registers_t* regs, uint64_t* call_stack) {

    
    
    if (panic_count >= MAX_KPANIK_COUNT) {
        Sys_Fatal("Double Fault (%lx) %lx\n", isr_index, regs->cr2);
        Sys_Info("Fix your shit\n");
        for (;;);
    }
    
    panic_count++;
    
    // task_switching_flag = false;
    


    Sys_log("Kernel panic (%lu | %lx | CR2:0x%lx)\n",
        (uint64_t)isr_index,
        (uint64_t)err_code,
        (uint64_t)regs->cr2);

    

    


    const char* error_name =
        (isr_index >= 0 &&
         isr_index < (int)(sizeof(crash_messages) / sizeof(crash_messages[0])))
            ? crash_messages[isr_index]
            : "Unknown Crash";

    Sys_log_NoPos("=======================================================================\n");
    Sys_log_NoPos("KERNEL PANIK -> ISR Index: %lu (%s), Error Code: %lx\n",
        isr_index, error_name, err_code);

    Sys_log_NoPos(" A critical error has occurred:\n");

    Sys_log_NoPos(" Error Code: %s (%03ld: %lx)\n",
        error_name, isr_index, err_code);

        
    Sys_log_NoPos("Error Bits:\n");
    
    _Log_Isr_Error_Code(isr_index, err_code);

    Sys_log_NoPos(" Regs Dump:\n");


    
    Sys_log_NoPos(" rax: 0x%lx  rbx: 0x%lx  rcx: 0x%lx  rdx: 0x%lx\n",
        regs->rax, regs->rbx,
        regs->rcx, regs->rdx);

    Sys_log_NoPos(" rsi: 0x%lx  rdi: 0x%lx  rbp: 0x%lx  rsp: 0x%lx\n",
        regs->rsi, regs->rdi,
        regs->rbp, regs->rsp);
        
        Sys_log_NoPos(" RIP: 0x%lx  RFLAGS: 0x%lx\n",
            regs->rip, regs->rflags);
            
    Sys_log_NoPos(" r8: 0x%lx  r9: 0x%lx  r10: 0x%lx  r11: 0x%lx\n",
        regs->r8, regs->r9,
        regs->r10, regs->r11);

    Sys_log_NoPos(" r12: 0x%lx  r13: 0x%lx  r14: 0x%lx  r15: 0x%lx\n",
        regs->r12, regs->r13, regs->r14, regs->r15);




    Sys_log_NoPos(" CS:  0x%04X  DS:  0x%04X  ES:  0x%04X\n",
        regs->cs, regs->ds, regs->es);

    Sys_log_NoPos(" FS:  0x%04X  GS:  0x%04X  SS:  0x%04X\n",
        regs->fs, regs->gs, regs->ss);

    Sys_log_NoPos(" CR0: 0x%lx  CR2: 0x%lx  CR3: 0x%lx CR4: 0x%lx CR8: 0x%lx\n",
        regs->cr0, regs->cr2, regs->cr3, regs->cr4, regs->cr8);

    Sys_log_NoPos(" CR3: 0x%lx\n", regs->cr3);

    asm volatile("sti");
    if (call_stack) {
        Sys_log_NoPos(" Call Stack Trace:\n");
        for (int i = 0; i < MAX_STACK_TRACE_SIZE; i++) {
            uintptr_t addr = call_stack[i];
            if (addr == 0) { 
                
                break;
            }
            // char tmp_buffer[64];
            Sys_log_NoPos("    %s (%p)\n","???", (void*)addr);
        }
    }

    for(;;);
    
}

void panic(const char* msg) {
    _manual_panic(msg,NULL);
}

void _manual_panic(const char* error, const char* info) {
    Sys_Fatal("kernel panic triggered!\n");
    Sys_Fatal("  Error: %s\n", error ? error : "(null)");
    Sys_Fatal("  Info : %s\n", info ? info : "(null)");

    if (error) {
        Sys_Error("%s",error);
    }

    if (info) {
        Sys_Info("%s",info);
    }

    Sys_log_NoPos(" Rebooting in 5 sec...\n");

    asm volatile("sti");

    printf("\n\n\n#");
    for (int i = 0; i < 4; i++) {
        printf("#");
        sleep_ms(1000);
    }

    // pc_reboot();
}

