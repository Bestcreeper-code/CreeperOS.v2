
#pragma once
#include <stdint.h>

#define KERNEL_CODE_SEGMENT 0x08
#define KERNEL_DATA_SEGMENT 0x10
#define USER_CODE_SEGMENT   0x23
#define USER_DATA_SEGMENT   0x1B
#define TSS_GDT_INDEX       5


struct gdt_entry64 {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_high;
} __attribute__((packed));


struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_0_15;
    uint8_t  base_16_23;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_24_31;
    uint32_t base_32_63;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr64 {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* 64-bit TSS */
struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

void init_gdt();
void init_tss(uintptr_t rsp0);
void setTss_sp(uintptr_t rsp0);