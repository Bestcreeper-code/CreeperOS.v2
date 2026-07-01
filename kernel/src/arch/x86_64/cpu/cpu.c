#include "cpu.h"

#include "debug/Logger.h"
#include "arch/x86_64/cpu/idt.h"
#include "string/format.h"
#include "string/string.h"

#include <stdint.h>
#include <cpuid.h>


#define FLAG_PER_ROW 8

short hfp;


char ecx_cpuid_1_flags[][16] = {
    "SSE3"       ,
    "PCLMUL"     ,
    "DTES64"     ,
    "MONITOR"    ,
    "DS_CPL"     ,
    "VMX"        ,
    "SMX"        ,
    "EST"        ,
    "TM2"        ,
    "SSSE3"      ,
    "CID"        ,
    "SDBG"       ,
    "FMA"        ,
    "CX16"       ,
    "XTPR"       ,
    "PDCM"       ,
    ""                          ,
    "PCID"       ,
    "DCA"        ,
    "SSE4_1"     ,
    "SSE4_2"     ,
    "X2APIC"     ,
    "MOVBE"      ,
    "POPCNT"     ,
    "TSC"        ,
    "AES"        ,
    "XSAVE"      ,
    "OSXSAVE"    ,
    "AVX"        ,
    "F16C"       ,
    "RDRAND"     ,
    "HYPERVISOR" ,
};


char edx_cpuid_1_flags[][8]= {
    "FPU"        ,
    "VME"        ,
    "DE"         ,
    "PSE"        ,
    "TSC"        ,
    "MSR"        ,
    "PAE"        ,
    "MCE"        ,
    "CX8"        ,
    "APIC"       ,
    ""                          ,
    "SEP"        ,
    "MTRR"       ,
    "PGE"        ,
    "MCA"        ,
    "CMOV"       ,
    "PAT"        ,
    "PSE36"      ,
    "PSN"        ,
    "CLFLUSH"    ,
    ""                          ,
    "DS"         ,
    "ACPI"       ,
    "MMX"        ,
    "FXSR"       ,
    "SSE"        ,
    "SSE2"       ,
    "SS"         ,
    "HTT"        ,
    "TM"         ,
    "IA64"       ,
    "PBE"               
};

char ebx_cpuid_7_flags[][16] = {
    "FSGSBASE",
    "TSC_ADJUST",
    "SGX",
    "BMI1",
    "HLE",
    "AVX2",
    "FDP_EXCPTN",
    "SMEP",
    "BMI2",
    "ERMS",
    "INVPCID",
    "RTM",
    "PQM",
    "FPU_CS_DS",
    "MPX",
    "PQE",
    "AVX512F",
    "AVX512DQ",
    "RDSEED",
    "ADX",
    "SMAP",
    "AVX512IFMA",
    "PCOMMIT",
    "CLFLUSHOPT",
    "CLWB",
    "INTEL_PT",
    "AVX512PF",
    "AVX512ER",
    "AVX512CD",
    "SHA",
    "AVX512BW",
    "AVX512VL"
};

uint64_t cpu_features = 0;
uint64_t cpu_features_ext = 0;

int cpu_log_specs(){
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    uint32_t name_string[4];

    name_string[3] = 0;
    __cpuid(0, eax, name_string[0], name_string[2], name_string[1]);//name and max 
    
    hfp = eax;

    Sys_Info("Cpu Manufacturer: %s\n", (char*)name_string);

    Sys_Info("Highest Function Parameter: %u\n", hfp);
        
    
    //fetch cache sizes
    int cache_index = 0;

    while (1) {
        __cpuid_count(4, cache_index, eax, ebx, ecx, edx);

        int cache_type = eax & 0x1F; 
        if (cache_type == 0)
            break; 

        int cache_level = (eax >> 5) & 0x7;
        if (cache_level <= 3) {
            unsigned int line_size = (ebx & 0xFFF) + 1;
            unsigned int partitions = ((ebx >> 12) & 0x3FF) + 1;
            unsigned int ways = ((ebx >> 22) & 0x3FF) + 1;
            unsigned int sets = ecx + 1;

            unsigned int cache_size = ways * partitions * line_size * sets;
            char cache_size_text[8];
            byte_nb_simplify(cache_size, cache_size_text,0);
            Sys_Info("L%d cache size = %s\n", cache_level, cache_size_text);
        }

        cache_index++;
    }

    __cpuid(0x1, eax, ebx, ecx, edx);

    unsigned int brand_index        = ebx & 0xFF;
    unsigned int clflush_line_size  = (ebx >> 8) & 0xFF;
    unsigned int apic_id            = (ebx >> 16) & 0xFF;
    unsigned int initial_apic_id    = (ebx >> 24) & 0xFF;

    Sys_Info(
        "BRAND INDEX: %x CLFLUSH_LINE_SIZE: %x APIC_ID: %x INITIAL_APIC_ID: %x\n",
        brand_index,
        clflush_line_size,
        apic_id,
        initial_apic_id
    );

    // ECX flags
    int count = 0;
    for (int i = 0; i < 32; i++) {
        if (ecx_cpuid_1_flags[i][0] == '\0')
            continue;

        int enabled = (ecx >> i) & 1;

        Sys_log("%s%s:%d\e[0m | ",
            enabled ? "\e[32m" : "\e[31m",
            ecx_cpuid_1_flags[i],
            enabled);

        count++;

        if (count % FLAG_PER_ROW == 0) Sys_log("\n");
    }

    if (count % FLAG_PER_ROW != 0) Sys_log("\n");



    // EDX flags
    count = 0;
    for (int i = 0; i < 32; i++) {
        if (edx_cpuid_1_flags[i][0] == '\0')
            continue;

        int enabled = (edx >> i) & 1;

        Sys_log("%s%s:%d\e[0m | ",
            enabled ? "\e[32m" : "\e[31m",
            edx_cpuid_1_flags[i],
            enabled);

        count++;

        if (count % FLAG_PER_ROW == 0) Sys_log("\n");
    }
    if (count % FLAG_PER_ROW != 0) Sys_log("\n");

    Sys_log_NoPos("\n");
        
    if (hfp >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
    
        Sys_log("[CPUID.7.0:EBX Features]\n");
    
        int count = 0;
        for (int i = 0; i < 32; i++) {
            if (ebx_cpuid_7_flags[i][0] == '\0')
                continue;
    
            int enabled = (ebx >> i) & 1;
    
            Sys_log("%s%s:%d\e[0m | ",
                enabled ? "\e[32m" : "\e[31m",
                ebx_cpuid_7_flags[i],
                enabled);
    
            count++;
    
            if (count % FLAG_PER_ROW == 0) Sys_log("\n");
        }
    
        if (count % FLAG_PER_ROW != 0) Sys_log("\n");
    }

    return 0;
}


void register_cpu_features() {
    uint32_t eax, ebx, ecx, edx;

    __cpuid(1, eax, ebx, ecx, edx);
    cpu_features = ((uint64_t)edx << 32) | ecx;

    if (eax >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);

        cpu_features_ext = ((uint64_t)ecx << 32) | ebx;
    }
}



extern void isr0();   // Divide Error
extern void isr1();   // Debug Exception
extern void isr2();   // Non Maskable Interrupt (NMI)
extern void isr3();   // Breakpoint Exception
extern void isr4();   // Overflow Exception
extern void isr5();   // Bound Range Exceeded Exception
extern void isr6();   // Invalid Opcode Exception
extern void isr7();   // Device Not Available Exception
extern void isr8();   // Double Fault Exception
extern void isr9();   // Coprocessor Segment Overrun (reserved)
extern void isr10();  // Invalid TSS Exception
extern void isr11();  // Segment Not Present Exception
extern void isr12();  // Stack-Segment Fault
extern void isr13();  // General Protection Fault
extern void isr14();  // Page Fault
extern void isr15();  // Reserved
extern void isr16();  // x87 Floating-Point Exception
extern void isr17();  // Alignment Check Exception
extern void isr18();  // Machine Check Exception
extern void isr19();  // SIMD Floating-Point Exception
extern void isr20();  // Virtualization Exception
extern void isr21();  // Control Protection Exception
extern void isr22();  // Reserved
extern void isr23();  // Reserved
extern void isr24();  // Reserved
extern void isr25();  // Reserved
extern void isr26();  // Reserved
extern void isr27();  // Reserved
extern void isr28();  // Hypervisor Injection Exception
extern void isr29();  // VMM Communication Exception
extern void isr30();  // Security Exception
extern void isr31();  // Reserved

void register_cpu_exceptions(){
    
    /* CPU exceptions 0–31 — same as before, just 64-bit addresses */
    idt_set_gate(0, (uintptr_t)isr0, 0x08, 0x8E);  idt_set_allocated(0);
    idt_set_gate(1, (uintptr_t)isr1, 0x08, 0x8E);  idt_set_allocated(1);
    idt_set_gate(2, (uintptr_t)isr2, 0x08, 0x8E);  idt_set_allocated(2);
    idt_set_gate(3, (uintptr_t)isr3, 0x08, 0x8E);  idt_set_allocated(3);
    idt_set_gate(4, (uintptr_t)isr4, 0x08, 0x8E);  idt_set_allocated(4);
    idt_set_gate(5, (uintptr_t)isr5, 0x08, 0x8E);  idt_set_allocated(5);
    idt_set_gate(6, (uintptr_t)isr6, 0x08, 0x8E);  idt_set_allocated(6);
    idt_set_gate(7, (uintptr_t)isr7, 0x08, 0x8E);  idt_set_allocated(7);
    idt_set_gate(8, (uintptr_t)isr8, 0x08, 0x8E);  idt_set_allocated(8);
    idt_set_gate(9, (uintptr_t)isr9, 0x08, 0x8E);  idt_set_allocated(9);
    idt_set_gate(10, (uintptr_t)isr10, 0x08, 0x8E);  idt_set_allocated(10);
    idt_set_gate(11, (uintptr_t)isr11, 0x08, 0x8E);  idt_set_allocated(11);
    idt_set_gate(12, (uintptr_t)isr12, 0x08, 0x8E);  idt_set_allocated(12);
    idt_set_gate(13, (uintptr_t)isr13, 0x08, 0x8E);  idt_set_allocated(13);
    idt_set_gate(14, (uintptr_t)isr14, 0x08, 0x8E);  idt_set_allocated(14);
    idt_set_gate(15, (uintptr_t)isr15, 0x08, 0x8E);  idt_set_allocated(15);
    idt_set_gate(16, (uintptr_t)isr16, 0x08, 0x8E);  idt_set_allocated(16);
    idt_set_gate(17, (uintptr_t)isr17, 0x08, 0x8E);  idt_set_allocated(17);
    idt_set_gate(18, (uintptr_t)isr18, 0x08, 0x8E);  idt_set_allocated(18);
    idt_set_gate(19, (uintptr_t)isr19, 0x08, 0x8E);  idt_set_allocated(19);
    idt_set_gate(20, (uintptr_t)isr20, 0x08, 0x8E);  idt_set_allocated(20);
    idt_set_gate(21, (uintptr_t)isr21, 0x08, 0x8E);  idt_set_allocated(21);
    idt_set_gate(22, (uintptr_t)isr22, 0x08, 0x8E);  idt_set_allocated(22);
    idt_set_gate(23, (uintptr_t)isr23, 0x08, 0x8E);  idt_set_allocated(23);
    idt_set_gate(24, (uintptr_t)isr24, 0x08, 0x8E);  idt_set_allocated(24);
    idt_set_gate(25, (uintptr_t)isr25, 0x08, 0x8E);  idt_set_allocated(25);
    idt_set_gate(26, (uintptr_t)isr26, 0x08, 0x8E);  idt_set_allocated(26);
    idt_set_gate(27, (uintptr_t)isr27, 0x08, 0x8E);  idt_set_allocated(27);
    idt_set_gate(28, (uintptr_t)isr28, 0x08, 0x8E);  idt_set_allocated(28);
    idt_set_gate(29, (uintptr_t)isr29, 0x08, 0x8E);  idt_set_allocated(29);
    idt_set_gate(30, (uintptr_t)isr30, 0x08, 0x8E);  idt_set_allocated(30);
    idt_set_gate(31, (uintptr_t)isr31, 0x08, 0x8E);  idt_set_allocated(31);

    
}   