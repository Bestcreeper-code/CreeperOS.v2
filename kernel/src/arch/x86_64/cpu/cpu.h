#pragma once


#include <stdint.h>
typedef enum {
    
    CPU_FEAT_SSE3        = 1ULL << 0,
    CPU_FEAT_PCLMUL      = 1ULL << 1,
    CPU_FEAT_DTES64      = 1ULL << 2,
    CPU_FEAT_MONITOR     = 1ULL << 3,
    CPU_FEAT_DS_CPL      = 1ULL << 4,
    CPU_FEAT_VMX         = 1ULL << 5,
    CPU_FEAT_SMX         = 1ULL << 6,
    CPU_FEAT_EST         = 1ULL << 7,
    CPU_FEAT_TM2         = 1ULL << 8,
    CPU_FEAT_SSSE3       = 1ULL << 9,
    CPU_FEAT_CID         = 1ULL << 10,
    CPU_FEAT_SDBG        = 1ULL << 11,

    CPU_FEAT_FMA         = 1ULL << 12,
    CPU_FEAT_CX16        = 1ULL << 13,
    CPU_FEAT_XTPR        = 1ULL << 14,
    CPU_FEAT_PDCM        = 1ULL << 15,

    CPU_FEAT_16          = 1ULL << 16, // reserved/undef
    CPU_FEAT_PCID        = 1ULL << 17,
    CPU_FEAT_DCA         = 1ULL << 18,
    CPU_FEAT_SSE4_1      = 1ULL << 19,
    CPU_FEAT_SSE4_2      = 1ULL << 20,
    CPU_FEAT_X2APIC      = 1ULL << 21,
    CPU_FEAT_MOVBE       = 1ULL << 22,
    CPU_FEAT_POPCNT      = 1ULL << 23,
    CPU_FEAT_TSC_DEADLINE= 1ULL << 24,
    CPU_FEAT_AES         = 1ULL << 25,
    CPU_FEAT_XSAVE       = 1ULL << 26,
    CPU_FEAT_OSXSAVE     = 1ULL << 27,
    CPU_FEAT_AVX         = 1ULL << 28,
    CPU_FEAT_F16C        = 1ULL << 29,
    CPU_FEAT_RDRAND      = 1ULL << 30,
    CPU_FEAT_HYPERVISOR  = 1ULL << 31,

    
    CPU_FEAT_FPU         = 1ULL << 32,
    CPU_FEAT_VME         = 1ULL << 33,
    CPU_FEAT_DE          = 1ULL << 34,
    CPU_FEAT_PSE         = 1ULL << 35,
    CPU_FEAT_TSC         = 1ULL << 36,
    CPU_FEAT_MSR         = 1ULL << 37,
    CPU_FEAT_PAE         = 1ULL << 38,
    CPU_FEAT_MCE         = 1ULL << 39,
    CPU_FEAT_CX8         = 1ULL << 40,
    CPU_FEAT_APIC        = 1ULL << 41,

    CPU_FEAT_42          = 1ULL << 42, // reserved
    CPU_FEAT_SEP         = 1ULL << 43,
    CPU_FEAT_MTRR        = 1ULL << 44,
    CPU_FEAT_PGE         = 1ULL << 45,
    CPU_FEAT_MCA         = 1ULL << 46,
    CPU_FEAT_CMOV        = 1ULL << 47,
    CPU_FEAT_PAT         = 1ULL << 48,
    CPU_FEAT_PSE36       = 1ULL << 49,
    CPU_FEAT_50          = 1ULL << 50, // reserved
    CPU_FEAT_CLFLUSH     = 1ULL << 51,
    CPU_FEAT_52          = 1ULL << 52, // reserved
    CPU_FEAT_DS          = 1ULL << 53,
    CPU_FEAT_ACPI        = 1ULL << 54,
    CPU_FEAT_MMX         = 1ULL << 55,
    CPU_FEAT_FXSR        = 1ULL << 56,
    CPU_FEAT_SSE         = 1ULL << 57,
    CPU_FEAT_SSE2        = 1ULL << 58,
    CPU_FEAT_SS          = 1ULL << 59,
    CPU_FEAT_HTT         = 1ULL << 60,
    CPU_FEAT_TM          = 1ULL << 61,
    CPU_FEAT_IA64        = 1ULL << 62,
    CPU_FEAT_PBE         = 1ULL << 63,

} cpu_feat;

extern uint64_t cpu_features;

int cpu_log_specs();
void register_cpu_features();

void register_cpu_exceptions();