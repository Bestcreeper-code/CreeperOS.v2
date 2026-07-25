#include "apic.h"

#include "arch/interrupts.h"
#include "arch/vmm.h"
#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/interrupts/pic.h"
#include "asm/asm.h"
#include "debug/Logger.h"
#include "interrupts/interrupts.h"
#include "interrupts/ioapic.h"
#include "memory/pmm.h"
#include "timer/timers.h"
#include "uACPI/include/uacpi/acpi.h"
#include "requests.h"
#include "defines/compiler_defs.h"
#include "string/string.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>




void* lapic_mmio_base;

static uint8_t bsp_lapic_id;

void _lapic_eoi(uint8_t vector) {
    lapic_mmio_write(LAPIC_REG_EOI, 0);
}

int lapic_init() {
    if(!(cpu_features & CPU_FEAT_APIC)) {
        Sys_Fatal("APIC Not Supported\n");
        return -1;
    }
    char* signature = rsdp_request.response->address;

    if(strncmp(signature, ACPI_RSDP_SIGNATURE, sizeof(ACPI_RSDP_SIGNATURE)-1)) {
        Sys_Fatal("RSDP Signature is wrong\n");
        return -1;
    }

    struct acpi_rsdp* rsdp = (struct acpi_rsdp*)signature;

    void* entries;
    size_t entry_count;
    bool is_extended;

    if(rsdp->revision < 2){
        struct acpi_rsdt* rsdt = (struct acpi_rsdt*)(uintptr_t)rsdp->rsdt_addr;
        entries =  rsdt->entries;
        entry_count = (rsdt->hdr.length - sizeof(rsdt->hdr)) / 4;
        is_extended = false;
    }
    else {
        struct acpi_xsdt* xsdt = (struct acpi_xsdt*)(uintptr_t) PHYS_2_HHDM(rsdp->xsdt_addr);
        entries = xsdt->entries;
        entry_count = (xsdt->hdr.length - sizeof(xsdt->hdr)) / 8;
        is_extended = true;
        
    }
    
    // Sys_log("%lx",(size_t)((uint64_t*)entries));
    
    for (size_t i = 0; i < entry_count; i++) {
        struct acpi_sdt_hdr* hdr;
    
        
        if (is_extended)
            hdr = (struct acpi_sdt_hdr*)(uintptr_t)PHYS_2_HHDM(((uint64_t*)entries)[i]);
        else
            hdr = (struct acpi_sdt_hdr*)(uintptr_t)PHYS_2_HHDM(((uint32_t*)entries)[i]);
        
        
        if(strncmp(hdr->signature, ACPI_MADT_SIGNATURE, sizeof(ACPI_MADT_SIGNATURE)-1) == 0) {
            struct acpi_madt* madt = (struct acpi_madt*)hdr;

            lapic_mmio_base = PHYS_2_HHDM( (physptr_t)(uintptr_t)madt->local_interrupt_controller_address );

            bsp_lapic_id = (uint8_t)((lapic_mmio_read(LAPIC_REG_ID) >> 24) & 0xFF);

            // MADT sub-entries searching the IOAPIC
            uint8_t* ptr = (uint8_t*)(madt + 1);
            uint8_t* end = (uint8_t*)madt + madt->hdr.length;

            while (ptr < end) {
                struct acpi_entry_hdr* ent_header = (struct acpi_entry_hdr*)ptr;
                if (ent_header->length == 0) break; 

                if (ent_header->type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
                    struct acpi_madt_ioapic* io = (struct acpi_madt_ioapic*)ent_header;
                    ioapic_init(io->address);

                    // dummy legacy routing fir test
                    ioapic_set_irq(2, 32, bsp_lapic_id, false);//pit (2 because for some fking reason it's changed to 2 while kbd stays same???)
                    ioapic_set_irq(1, 33, bsp_lapic_id, false);//kbd

                    Sys_Success("IOAPIC initialized\n");
                }

                ptr += ent_header->length;
            }

            block_pic();

            lapic_mmio_write(LAPIC_REG_SIVR, lapic_mmio_read(LAPIC_REG_SIVR) | 0x100);// start receiving interrupts (bit 8)

            _current_eoi = _lapic_eoi;

            lapic_timer_init(LAPIC_TIMER_MODE_PERIODIC, 0x1000, 16);// div by 16

            Sys_Success("APIC initialized\n");
        }
    }
}


uint8_t lapic_interrupt_vector = 0;
int8_t lapic_timer_id = -1;

extern void apic_timer_interrupt_handler();

void c_apic_timer_interrupt_handler(register_t sp) {
    
    common_timer_dispatcher(lapic_timer_id, sp);
    _lapic_eoi(lapic_interrupt_vector);
}

void lapic_timer_reset_interrupt(timer_dev* dev) {
    lapic_mmio_write(LAPIC_REG_EOI, 0);
}

timer_dev lapic_timer_dev = {
    .name = "lapic_timer",
    .freq = 0, // not used for time anyway
    .type = TIMER_DEV_PERIODIC,

    .vector = 0, //placeholder
    .flags = 0,

    .enable = NULL,
    .disable = NULL,
    .gettime_us = NULL,
    .settime = NULL,
    .reset_interrupt = lapic_timer_reset_interrupt,
};

// rets the int vector
uint8_t lapic_timer_init(uint8_t mode, uint32_t initial_count, uint32_t divide_config)
{
    Sys_Info("Setting up APIC timer\n");

    uint16_t vector = allocate_interrupt_vector();

    if (vector > 0xFF) {
        Sys_Fatal("Invalid LAPIC vector\n");
        return 0;
    }

    lapic_interrupt_vector = (uint8_t)vector;

    setup_interrupt_vector((uint8_t)vector, apic_timer_interrupt_handler, IRQ_FLAG_INTERRUPT);

    //mask timer
    uint32_t lvt = 1 << 16; 

    //set mode 
    if (mode == LAPIC_TIMER_MODE_PERIODIC)
        lvt |= (1u << 17);

    //  set vector 
    lvt |= (vector & 0xFF);

    lapic_mmio_write(LAPIC_REG_LVT_TIMER, lvt);

    
    switch (divide_config) {
        case 2:  divide_config = 0x0; break;
        case 4:  divide_config = 0x1; break;
        case 8:  divide_config = 0x2; break;
        case 16: divide_config = 0x3; break;
        case 1:  divide_config = 0xB; break;
        default:
            divide_config = 0x3; 
            break;
    }

    lapic_mmio_write(LAPIC_REG_DIV, divide_config);

    
    lapic_mmio_write(LAPIC_REG_INIT_CNT, initial_count);//load counter

    lapic_timer_id = timer_register(&lapic_timer_dev, TIMER_ROLE_SCHED, 1000);

    lapic_mmio_write(LAPIC_REG_SIVR, 0x1FF);

    //unmask timer 
    lvt &= ~(1u << 16);
    lapic_mmio_write(LAPIC_REG_LVT_TIMER, lvt);

    Sys_Success("APIC timer set up\n");
    return lapic_interrupt_vector;
}