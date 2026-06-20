#include "pci.h"

#include "debug/Logger.h"
#include "drivers/drivers.h"
#include "pci_config_io.h"

#include <stddef.h>
#include <stdint.h>


struct pci_device_mapping_entry {
    uint16_t vendor_id;
    uint16_t device_id;
    char     vendor_name[20];
    char     device_name[48];
};

static const struct pci_device_mapping_entry pci_device_mappings_list[] = {
    { 0x1AF4, 0x1000, "Red Hat", "VirtIO Network"              },
    { 0x1AF4, 0x1001, "Red Hat", "VirtIO Block"                },
    { 0x1AF4, 0x1002, "Red Hat", "VirtIO Balloon"              },
    { 0x1AF4, 0x1003, "Red Hat", "VirtIO Console"              },
    { 0x1AF4, 0x1004, "Red Hat", "VirtIO SCSI"                 },
    { 0x1AF4, 0x1005, "Red Hat", "VirtIO RNG"                  },
    { 0x1AF4, 0x1009, "Red Hat", "VirtIO 9P (filesystem)"      },
    { 0x1AF4, 0x1010, "Red Hat", "VirtIO GPU"                  },
    { 0x1AF4, 0x1011, "Red Hat", "VirtIO Input"                },
    { 0x1AF4, 0x1012, "Red Hat", "VirtIO Socket (vsock)"       },
    { 0x1AF4, 0x1014, "Red Hat", "VirtIO Crypto"               },
    { 0x1AF4, 0x1041, "Red Hat", "VirtIO Network (modern)"     },
    { 0x1AF4, 0x1042, "Red Hat", "VirtIO Block (modern)"       },
    { 0x1AF4, 0x1043, "Red Hat", "VirtIO Console (modern)"     },
    { 0x1AF4, 0x1044, "Red Hat", "VirtIO RNG (modern)"         },
    { 0x1AF4, 0x1045, "Red Hat", "VirtIO Balloon (modern)"     },
    { 0x1AF4, 0x1048, "Red Hat", "VirtIO SCSI (modern)"        },
    { 0x1AF4, 0x1049, "Red Hat", "VirtIO 9P (modern)"          },
    { 0x1AF4, 0x1050, "Red Hat", "VirtIO GPU (modern)"         },
    { 0x1AF4, 0x1052, "Red Hat", "VirtIO Input (modern)"       },
    { 0x1AF4, 0x1053, "Red Hat", "VirtIO Socket (modern)"      },

    
    { 0x1B36, 0x0001, "QEMU", "PCI-PCI Bridge"                 },
    { 0x1B36, 0x0002, "QEMU", "PCI Serial"                     },
    { 0x1B36, 0x0003, "QEMU", "PCI Serial (2x)"                },
    { 0x1B36, 0x0004, "QEMU", "PCI Serial (4x)"                },
    { 0x1B36, 0x0005, "QEMU", "PCI Test"                       },
    { 0x1B36, 0x0006, "QEMU", "PCI Rocker Switch"              },
    { 0x1B36, 0x0007, "QEMU", "PCI xHCI USB 3.0"               },
    { 0x1B36, 0x0008, "QEMU", "PCIe Host"                      },
    { 0x1B36, 0x0009, "QEMU", "PCIe-PCI Bridge"                },
    { 0x1B36, 0x000A, "QEMU", "PCIe Graphics"                  },
    { 0x1B36, 0x000B, "QEMU", "PCIe Root Port"                 },
    { 0x1B36, 0x000D, "QEMU", "PCI IVSHMEM"                    },
    { 0x1B36, 0x000E, "QEMU", "NVMe"                           },
    { 0x1B36, 0x000F, "QEMU", "PVPANIC"                        },
    { 0x1B36, 0x0010, "QEMU", "PCIe GPEx Root Port"            },

    
    { 0x8086, 0x1237, "Intel", "440FX PCI-Host (PIIX)"         },
    { 0x8086, 0x7000, "Intel", "PIIX3 ISA Bridge"              },
    { 0x8086, 0x7010, "Intel", "PIIX3 IDE"                     },
    { 0x8086, 0x7020, "Intel", "PIIX3 USB 1.1 (UHCI)"          },
    { 0x8086, 0x7113, "Intel", "PIIX4 ACPI / Power Mgmt"       },
    { 0x8086, 0x29C0, "Intel", "Q35 Host Bridge (MCH)"         },
    { 0x8086, 0x2918, "Intel", "ICH9 LPC Bridge"               },
    { 0x8086, 0x2922, "Intel", "ICH9 AHCI SATA"                },
    { 0x8086, 0x2930, "Intel", "ICH9 SMBus"                    },
    { 0x8086, 0x2934, "Intel", "ICH9 EHCI USB 2.0 (port 1)"   },
    { 0x8086, 0x293A, "Intel", "ICH9 EHCI USB 2.0 (port 2)"   },
    { 0x8086, 0x293E, "Intel", "ICH9 HD Audio"                 },
    { 0x8086, 0x2415, "Intel", "ICH AC97 Audio"                },
    { 0x8086, 0x100E, "Intel", "82540EM Gigabit NIC (e1000)"   },
    { 0x8086, 0x10D3, "Intel", "82574L Gigabit NIC (e1000e)"   },
    { 0x8086, 0x282A, "Intel", "SATA RST RAID Ctrl"            },
    { 0x8086, 0x3482, "Intel", "LP LPC Ctrl"                   },
    { 0x8086, 0x34A3, "Intel", "LP SMBus Ctrl"                 },
    { 0x8086, 0x34A4, "Intel", "LP SPI Ctrl"                   },
    { 0x8086, 0x34B4, "Intel", "LP PCIe Port #13"              },
    { 0x8086, 0x34BD, "Intel", "LP PCIe Port #6"               },
    { 0x8086, 0x34C8, "Intel", "LP SmartSound Audio"           },
    { 0x8086, 0x34E0, "Intel", "LP Management Engine"          },
    { 0x8086, 0x34E8, "Intel", "LP Serial IO I2C #0"           },
    { 0x8086, 0x34E9, "Intel", "LP Serial IO I2C #1"           },
    { 0x8086, 0x34ED, "Intel", "LP USB3 xHCI Ctrl"             },
    { 0x8086, 0x34EF, "Intel", "LP DRAM Ctrl"                  },
    { 0x8086, 0x34F0, "Intel", "LP CNVi WiFi"                  },
    { 0x8086, 0x34FC, "Intel", "LP Sensor Hub"                 },
    { 0x8086, 0x8A03, "Intel", "Pwr/Thermal Ctrl"              },
    { 0x8086, 0x8A12, "Intel", "IceLake Host Bridge"           },
    { 0x8086, 0x8A13, "Intel", "Thunderbolt USB Ctrl"          },
    { 0x8086, 0x8A17, "Intel", "Thunderbolt NHI"               },
    { 0x8086, 0x8A1D, "Intel", "Thunderbolt Root Port #0"      },
    { 0x8086, 0x8A1F, "Intel", "Thunderbolt Root Port #1"      },
    { 0x8086, 0x8A52, "Intel", "Iris Plus G7 GPU"              },




    { 0x10EC, 0x525A, "Realtek", "RTS525A Card Reader"         },



    
    { 0x144D, 0xA808, "Samsung", "NVMe SM981/PM981/PM983"      },


    
    { 0x1234, 0x1111, "Technical Corp", "QEMU Standard VGA"    },

    
    
    { 0x15AD, 0x0405, "VMware", "SVGA II (Display)"            },
    { 0x15AD, 0x0740, "VMware", "VMCI"                         },
    { 0x15AD, 0x0770, "VMware", "USB 2.0 EHCI"                 },
    { 0x15AD, 0x0774, "VMware", "USB 3.0 xHCI"                 },
    { 0x15AD, 0x07A0, "VMware", "PCIe Root Port"               },
    { 0x15AD, 0x07B0, "VMware", "VMXNET3 NIC"                  },
    { 0x15AD, 0x07C0, "VMware", "PVSCSI"                       },
    { 0x15AD, 0x07E0, "VMware", "SATA AHCI"                    },
    { 0x15AD, 0x0801, "VMware", "VM Interface (backdoor)"      },
    { 0x15AD, 0x1977, "VMware", "HD Audio"                     },

    
    { 0x80EE, 0xBEEF, "VirtualBox", "VGA"                      },
    { 0x80EE, 0xCAFE, "VirtualBox", "VMMDev (Guest Service)"   },

    
    { 0x1414, 0x5353, "Microsoft", "Hyper-V VMBus"             },
    { 0x1414, 0x0001, "Microsoft", "Hyper-V Virtual IDE"       },
    { 0x1414, 0x0002, "Microsoft", "Hyper-V Synthetic SCSI"    },
    { 0x1414, 0x0003, "Microsoft", "Hyper-V Synthetic NIC"     },
    { 0x1414, 0x0004, "Microsoft", "Hyper-V Video (Basic)"     },
    { 0x1414, 0x0006, "Microsoft", "Hyper-V Keyboard"          },
    { 0x1414, 0x0009, "Microsoft", "Hyper-V NVMe"              },

    
    { 0x0000, 0x0000, "unknown", "unknown" }
};

const struct pci_device_mapping_entry* pci_device_lookup(uint16_t vendor_id, uint16_t device_id) {
    for (size_t i = 0;
         i < (sizeof(pci_device_mappings_list) / sizeof(pci_device_mappings_list[0])-1);
         i++) {
        if (pci_device_mappings_list[i].vendor_id == vendor_id &&
            pci_device_mappings_list[i].device_id == device_id)
            return &pci_device_mappings_list[i];
    }
    return &pci_device_mappings_list[sizeof(pci_device_mappings_list) / sizeof(pci_device_mappings_list[0])];
}


uint16_t get_vendor_id(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read16(bus, device, function, 0x00);
}

uint16_t get_device_id(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read16(bus, device, function, 0x02);
}

uint8_t get_header_type(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t value = pci_config_read16(bus, device, function, 0x0E);
    return (uint8_t)(value & 0x00FF);
}

void checkFunction(uint8_t bus, uint8_t device, uint8_t function) {
}


void check_device(uint8_t bus, uint8_t device) {
    uint8_t function = 0;

    uint16_t vendorID = get_vendor_id(bus, device, function);
    if (vendorID == 0xFFFF) return;

    uint16_t deviceID = get_device_id(bus, device, function);

#if PCI_DEBUG
    const struct pci_device_mapping_entry* devinfo =
        pci_device_lookup(vendorID, deviceID);

    Sys_log("(%u:%u.%u) vendor_id=%04x (%s) dev_id=%04x (%s)\n",
        (uint32_t)bus, (uint32_t)device, (uint32_t)function,
        (uint32_t)vendorID,
        devinfo ? devinfo->vendor_name : "unknown",
        (uint32_t)deviceID,
        devinfo ? devinfo->device_name : "unknown");
#endif

    pci_driver* driver = get_pci_driver(vendorID, deviceID);
    if (driver) {
        Sys_Info("calling driver %s for (%u:%u.%u)\n",
            driver->name, bus, device, function);
        driver->init(bus, device, function);
    }

    //multifunction?
    uint8_t headerType = get_header_type(bus, device, function);
    if ((headerType & 0x80) == 0)
        return;

    
    for (function = 1; function < 8; function++) {
        uint16_t vId = get_vendor_id(bus, device, function);
        if (vId == 0xFFFF)
            continue;

        uint16_t dId = get_device_id(bus, device, function);

#if PCI_DEBUG
        const struct pci_device_mapping_entry* devinfo =
            pci_device_lookup(vId, dId);

        Sys_log("(%u:%u.%u) vendor_id=%04x (%s) dev_id=%04x (%s)\n",
            (uint32_t)bus, (uint32_t)device, (uint32_t)function,
            (uint32_t)vId,
            devinfo ? devinfo->vendor_name : "unknown",
            (uint32_t)dId,
            devinfo ? devinfo->device_name : "unknown");
#endif

        pci_driver* driver = get_pci_driver(vId, dId);
        if (driver) {
            Sys_Info("calling driver %s for (%u:%u.%u)\n",
                driver->name, bus, device, function);
            driver->init(bus, device, function);
        }
    }
}


REGISTER_DRIVER_DEV(pci, check_all_buses);
int check_all_buses() {
    uint16_t bus;
    uint8_t device;

    for (bus = 0; bus < 256; bus++) {
        for (device = 0; device < 32; device++) {
            check_device(bus, device);
        }
    }
}

pci_driver* get_pci_driver(uint16_t vendor_id, uint16_t device_id){
    for(pci_driver* driver = __start_pci_drivers; 
        driver < __stop_pci_drivers; driver++) {

        if(driver->vendor_id == vendor_id  
            && driver->device_id == device_id ) {

            return driver;
        }
    }
    return NULL;
}