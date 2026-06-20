#include "pic.h"
#include "debug/Logger.h"
#include "asm/ams.h"

void pic_remap() {
    Sys_Info("remapping PIC...\n");

    
    outb(0x20, 0x11); // Start initialization (master PIC)
    outb(0xA0, 0x11); // Start initialization (slave PIC)

    outb(0x21, 0x20); // Remap master PIC vector offset to 0x20 (32)
    outb(0xA1, 0x28); // Remap slave PIC vector offset to 0x28 (40)

    outb(0x21, 0x04); // Tell master PIC there is a slave at IRQ2
    outb(0xA1, 0x02); // Tell slave PIC its cascade identity

    outb(0x21, 0x01); // Set 8086 mode for master PIC
    outb(0xA1, 0x01); // Set 8086 mode for slave PIC

    outb(0x21, 0x0);  // Clear master PIC mask (enable all IRQs)
    outb(0xA1, 0x0);  // Clear slave PIC mask (enable all IRQs)

    
    Sys_Success("PIC remapped\n");
}