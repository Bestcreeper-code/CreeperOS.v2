#include "AHCI.h"



#include "debug/Logger.h"
#include "drivers/PCI/pci.h"
#include "memops.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#define AHCI_DEBUG 1

int AHCI_init(uint8_t bus, uint8_t device, uint8_t function) {
    Sys_Warning("AHCI_init called for (%u:%u.%u)", bus, device, function);
    return 0;
}

REGISTER_PCI_DRIVER(ahci_intel, AHCI_init, 0x8086, 0x2922);
REGISTER_PCI_DRIVER(ahci_amd,   AHCI_init, 0x1022, 0x7901);
