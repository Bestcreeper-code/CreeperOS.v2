


#include "arch/x86_64/cpu/idt.h"
#include "Debug/Logger.h"
#include "drivers/PS-2/mouse.h"
#include "memops.h"
#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_reg;

static inline void idt_flush64(uint64_t ptr) {
    __asm__ volatile("lidt (%0)" :: "r"(ptr));
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
}

/* extern declarations from isrs.asm / irq handlers */
extern void isr0();   // Divide Error
extern void isr1();   // Debug Exception
extern void isr2();   // Non Maskable Interrupt (NMI)
extern void isr3();   // Breakpoint Exception
extern void isr4();   // Overflow Exception
extern void isr5();   // Bound Range Exceeded Exception
extern void isr6();   // Invalid Opcode Exception
extern void isr7();   // Device Not Available Exception
extern void isr8();   // Double Fault Exception
extern void isr9();   // Coprocessor Segment Overrun (reserved)
extern void isr10();  // Invalid TSS Exception
extern void isr11();  // Segment Not Present Exception
extern void isr12();  // Stack-Segment Fault
extern void isr13();  // General Protection Fault
extern void isr14();  // Page Fault
extern void isr15();  // Reserved
extern void isr16();  // x87 Floating-Point Exception
extern void isr17();  // Alignment Check Exception
extern void isr18();  // Machine Check Exception
extern void isr19();  // SIMD Floating-Point Exception
extern void isr20();  // Virtualization Exception
extern void isr21();  // Control Protection Exception
extern void isr22();  // Reserved
extern void isr23();  // Reserved
extern void isr24();  // Reserved
extern void isr25();  // Reserved
extern void isr26();  // Reserved
extern void isr27();  // Reserved
extern void isr28();  // Hypervisor Injection Exception
extern void isr29();  // VMM Communication Exception
extern void isr30();  // Security Exception
extern void isr31();  // Reserved

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

    /* CPU exceptions 0–31 — same as before, just 64-bit addresses */
    idt_set_gate(0, (uintptr_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uintptr_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uintptr_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uintptr_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uintptr_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uintptr_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uintptr_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uintptr_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uintptr_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uintptr_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uintptr_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uintptr_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uintptr_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uintptr_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uintptr_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uintptr_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uintptr_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uintptr_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uintptr_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uintptr_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uintptr_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uintptr_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uintptr_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uintptr_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uintptr_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uintptr_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uintptr_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uintptr_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uintptr_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uintptr_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uintptr_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uintptr_t)isr31, 0x08, 0x8E);


    for (int i = 32; i <= 47; i++)
        idt_set_gate(i, (uintptr_t)irq_dummy_handler, 0x08, 0x8E);

    // idt_set_gate(32, (uintptr_t)irq0_handler,  0x08, 0x8E); // PIT
    // idt_set_gate(33, (uintptr_t)irq1_handler,  0x08, 0x8E); // KB
    // // init_keyboard(); reset_input_buffer();
    // idt_set_gate(44, (uintptr_t)irq12_handler, 0x08, 0x8E); // Mouse
    init_mouse();

    
    // idt_set_gate(0x80, (uintptr_t)_syscall_i nt_80_handler, 0x08, 0xEE); // syscall

    idt_flush64((uint64_t)(uintptr_t)&idt_reg);
    Sys_Success("IDT (64-bit) set up.\n");
}