#pragma once

#include <stdint.h>



uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);



void pci_config_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);

void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
