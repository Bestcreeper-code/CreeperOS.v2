#include "cpu.h"

#include "Debug/Logger.h"
#include "string/format.h"
#include "string/string.h"

#include <stdint.h>
#include <cpuid.h>


#define FLAG_PER_ROW 8

short hfp;


char ecx_cpuid_1_flags[][32] = {
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


char edx_cpuid_1_flags[][32]= {
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

char ebx_cpuid_7_flags[][32] = {
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
