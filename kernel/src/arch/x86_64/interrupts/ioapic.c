#include "ioapic.h"
#include "arch/vmm.h"   // PHYS_2_HHDM
#include "memory/pmm.h"
#include <stdbool.h>

#define IOAPIC_REGSEL 0x00
#define IOAPIC_REGWIN 0x10
#define IOAPIC_REG_VER    0x01
#define IOAPIC_REG_REDTBL 0x10

physptr_t ioapic_base;


static inline uint32_t ioapic_read(uint8_t reg) {
    *(volatile uint32_t*)((uint8_t*)ioapic_base + IOAPIC_REGSEL) = reg;
    return *(volatile uint32_t*)((uint8_t*)ioapic_base + IOAPIC_REGWIN);
}

static inline void ioapic_write(uint8_t reg, uint32_t val) {
    *(volatile uint32_t*)((uint8_t*)ioapic_base + IOAPIC_REGSEL) = reg;
    *(volatile uint32_t*)((uint8_t*)ioapic_base + IOAPIC_REGWIN) = val;
}

void ioapic_init(uintptr_t ioapic_phys_base) {
    ioapic_base = (physptr_t)PHYS_2_HHDM(ioapic_phys_base);

    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    uint8_t max_entries = (uint8_t)((ver >> 16) & 0xFF);

    for (uint8_t i = 0; i <= max_entries; i++) {
        uint32_t low = ioapic_read(IOAPIC_REG_REDTBL + i*2);
        low |= (1 << 16);
        ioapic_write(IOAPIC_REG_REDTBL + i*2, low);
    }
}

void ioapic_set_irq(uint8_t irq, uint8_t vector, uint8_t lapic_id, bool masked) {
    uint32_t low  = vector;
    uint32_t high = ((uint32_t)lapic_id) << 24;
    if (masked) low |= (1 << 16);

    ioapic_write(IOAPIC_REG_REDTBL + irq*2 + 1, high);
    ioapic_write(IOAPIC_REG_REDTBL + irq*2,     low);
}