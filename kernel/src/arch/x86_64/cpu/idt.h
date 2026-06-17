
#pragma once
#include <stdint.h>

/* 64-bit interrupt gate — 16 bytes */
struct idt_entry {
    uint16_t offset_0_15;
    uint16_t selector;
    uint8_t  ist;        /* bits 2:0 = IST index (0 = legacy stack switch) */
    uint8_t  type_attr;  /* 0x8E = present, DPL=0, 64-bit interrupt gate */
    uint16_t offset_16_31;
    uint32_t offset_32_63;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init();
void idt_set_gate(uint8_t num, uintptr_t base, uint16_t sel, uint8_t flags);

void idt_set_allocated(uint8_t idx);