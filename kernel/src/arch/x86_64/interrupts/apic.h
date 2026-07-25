#pragma once

#include <stdbool.h>
#pragma once
#include <stdint.h>

#define LAPIC_REG_ID        0x020
#define LAPIC_REG_VERSION   0x030
#define LAPIC_REG_TPR       0x080
#define LAPIC_REG_APR       0x090
#define LAPIC_REG_PPR       0x0A0
#define LAPIC_REG_EOI       0x0B0
#define LAPIC_REG_LDR       0x0D0
#define LAPIC_REG_DFR       0x0E0
#define LAPIC_REG_SIVR      0x0F0

#define LAPIC_REG_ISR       0x100
#define LAPIC_REG_TMR       0x180
#define LAPIC_REG_IRR       0x200

#define LAPIC_REG_ESR       0x280

#define LAPIC_REG_ICR_LOW   0x300
#define LAPIC_REG_ICR_HIGH  0x310

#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_LVT_LINT0 0x350
#define LAPIC_REG_LVT_LINT1 0x360
#define LAPIC_REG_LVT_ERROR 0x370

#define LAPIC_REG_INIT_CNT  0x380
#define LAPIC_REG_CUR_CNT   0x390

#define LAPIC_REG_DIV       0x3E0


#define LAPIC_TIMER_MODE(mode) ((mode & 0x3) << 17)

#define LAPIC_TIMER_MODE_ONE_SHOT 0
#define LAPIC_TIMER_MODE_PERIODIC 1


#define ISO_POLARITY_MASK   0x3
#define ISO_POLARITY_LOW    0x3  
#define ISO_TRIGGER_MASK    0xC
#define ISO_TRIGGER_LEVEL   0xC  

typedef struct {
    uint8_t  gsi;
    bool     active_low;
    bool     level_triggered;
    bool     valid;
} irq_override_t;

static irq_override_t isa_irq_override[16];



extern void* lapic_mmio_base;

static inline volatile uint32_t* lapic_reg(uint32_t offset) {
    return (volatile uint32_t*)(lapic_mmio_base + offset);
}

static inline uint32_t lapic_mmio_read(uint32_t offset) {
    return *lapic_reg(offset);
}

static inline void lapic_mmio_write(uint32_t offset, uint32_t value) {
    *lapic_reg(offset) = value;
}

static inline void lapic_eoi(){
    lapic_mmio_write(LAPIC_REG_EOI, 0);
}



int lapic_init();



uint8_t lapic_timer_init( uint8_t mode, uint32_t initial_count, uint32_t divide_config);