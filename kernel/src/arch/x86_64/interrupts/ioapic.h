#pragma once
#include <stdbool.h>
#include <stdint.h>

void ioapic_init(uintptr_t ioapic_phys_base);
void ioapic_set_irq(uint8_t irq, uint8_t vector, uint8_t lapic_id, bool masked);