#pragma once



#include <stdint.h>



short allocate_interrut_vector();
void free_interrut_vector(uint16_t idx);

typedef enum {
    // IRQ_FLAG_PRESENT = (1 << 0),
    IRQ_FLAG_USER       = (1 << 1),
    IRQ_FLAG_INTERRUPT  = (0 << 2),
    IRQ_FLAG_TRAP       = (1 << 2),
} irq_flags;

int setup_interrupt_vector(short vec, void* handler, irq_flags flags);
