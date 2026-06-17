


#include "arch/x86_64/cpu/idt.h"
#include "Debug/Logger.h"
#include "arch/interrupts.h"
#include "defines/helpers.h"

#include "memops.h"
#include <stdint.h>


#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_reg;

uint64_t idt_vectors_bitmap[IDT_ENTRIES/64];


static inline void idt_flush(uint64_t ptr) {
    __asm__ volatile("lidt (%0)" :: "r"(ptr));
}

void idt_set_allocated(uint8_t idx) {
    bitmap_set((char*)idt_vectors_bitmap, idx, 0);
}

short allocate_interrut_vector() {
    return (short)wbitmap_alloc_1_first((char*)idt_vectors_bitmap, sizeof(idt_vectors_bitmap));
}

void free_interrupt_vector(uint16_t idx) {
    idt_set_gate(idx, 0, 0, 0);
    bitmap_set((char*)idt_vectors_bitmap, idx, 0);
}

void idt_set_gate(uint8_t num, uintptr_t base, uint16_t sel, uint8_t flags)
{
    idt[num].offset_0_15  = base & 0xFFFF;
    idt[num].selector     = sel;
    idt[num].ist          = 0;
    idt[num].type_attr    = flags;
    idt[num].offset_16_31 = (base >> 16) & 0xFFFF;
    idt[num].offset_32_63 = (uint32_t)(base >> 32);
    idt[num].reserved     = 0;

    idt_flush((uint64_t)(uintptr_t)&idt_reg);
}


// extern void irq0_handler();
// extern void irq1_handler();
// extern void irq12_handler();

extern void irq_dummy_handler();
// extern void _syscall_int_80_handler();

void idt_init()
{
    idt_reg.limit = sizeof(idt) - 1;
    idt_reg.base  = (uint64_t)(uintptr_t)idt;
    memset(idt, 0, sizeof(idt));

    idt_flush((uint64_t)(uintptr_t)&idt_reg);
    Sys_Success("IDT set up.\n");
}




int setup_interrupt_vector(short vec, void* handler, irq_flags flags) {
    uint8_t attr = 0x8E;

    if (flags & IRQ_FLAG_USER) attr = (attr & ~0x60) | 0x60;//DPL3
    if (flags & IRQ_FLAG_TRAP) attr = (attr & ~0x0F) | 0x0F;//trap gate

    idt_set_gate(vec, (uintptr_t)handler, 0x08, attr);
    return 0;
}