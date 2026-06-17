#pragma once

#include "defines/compiler_defs.h"

#include <stdint.h>


typedef struct {
    const char *name;
    uint16_t vendor_id;
    uint16_t device_id;
    int (*init)(uint8_t bus, uint8_t device, uint8_t function);
} pci_driver;

extern pci_driver __start_pci_drivers[], __stop_pci_drivers[];

#define REGISTER_PCI_DRIVER(d_name, init_f, vend_id, dev_id ) \
    GCC_ATTR((section("pci_drivers"), used, aligned(8))) \
    static pci_driver __##d_name##_driver_struct = {.name=#d_name, \
        .init=init_f, .vendor_id=vend_id, .device_id = dev_id}


uint16_t get_vendor_id(uint8_t bus, uint8_t device, uint8_t function);

int check_all_buses();


pci_driver* get_pci_driver(uint16_t vendor_id,uint16_t  device_id);