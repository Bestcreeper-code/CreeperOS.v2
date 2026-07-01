#include "apic.h"

#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/interrupts/pic.h"
#include "debug/Logger.h"
#include "uACPI/include/uacpi/acpi.h"
#include "requests.h"
#include "uACPI/include/uacpi/uacpi.h"
#include "string/string.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>




void* lapic_mmio_base;


int lapic_init() {
    if(cpu_features & CPU_FEAT_APIC) {
        Sys_Fatal("APIC Not Supported");
        return -1;
    }
    char* signature = rsdp_request.response->address;

    if(strncmp(signature, ACPI_RSDP_SIGNATURE, sizeof(ACPI_RSDP_SIGNATURE)-1)) {
        Sys_Fatal("RSDP Signature is wrong");
        return -1;
    }

    struct acpi_rsdp* rsdp = (struct acpi_rsdp*)signature;

    void* entries;
    size_t entry_count;
    bool is_extended;

    if(rsdp->revision < 2){
        struct acpi_rsdt* rsdt = (struct acpi_rsdt*)(uintptr_t)rsdp->rsdt_addr;
        entries = rsdt->entries;
        entry_count = (rsdt->hdr.length - sizeof(rsdt->hdr)) / 4;
        is_extended = false;
    }
    else {
        struct acpi_xsdt* xsdt = (struct acpi_xsdt*)(uintptr_t)rsdp->xsdt_addr;
        entries = xsdt->entries;
        entry_count = (xsdt->hdr.length - sizeof(xsdt->hdr)) / 8;
        is_extended = true;
    }
    // mp_request.response->bsp_lapic_id
    for (size_t i = 0; i < entry_count; i++) {
        struct acpi_sdt_hdr* hdr;
    
        if (is_extended)
            hdr = (struct acpi_sdt_hdr*)(uintptr_t)((uint64_t*)entries)[i];
        else
            hdr = (struct acpi_sdt_hdr*)(uintptr_t)((uint32_t*)entries)[i];

        if(strncmp(hdr->signature, ACPI_MADT_SIGNATURE, sizeof(ACPI_MADT_SIGNATURE)-1) == 0) {
            struct acpi_madt* madt = (struct acpi_madt*)hdr;

            lapic_mmio_base = (void*)(uintptr_t)madt->local_interrupt_controller_address;

            block_pic();
            
            lapic_mmio_write(LAPIC_REG_SIVR, lapic_mmio_read(LAPIC_REG_SIVR) | 0x100);// start receiving interrupts (bit 8)
            

        }
    }

    

}