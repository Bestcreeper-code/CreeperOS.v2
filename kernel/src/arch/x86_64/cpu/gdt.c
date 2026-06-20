#include "arch/x86_64/cpu/gdt.h"
#include "debug/Logger.h"
#include "memops.h"


extern void gdt_flush(uint64_t);   /* in asm/gdt.asm */

/* 7 slots: null, kcode, kdata, udata, ucode, tss_lo, tss_hi */
static struct gdt_entry64 gdt[7];
static struct gdt_ptr64   gdt_ptr;
static struct tss64        tss;

static void set_gate(int idx, uint32_t base, uint32_t limit,
                     uint8_t access, uint8_t flags)
{
    gdt[idx].limit_low   = limit & 0xFFFF;
    gdt[idx].base_low    = base  & 0xFFFF;
    gdt[idx].base_mid    = (base >> 16) & 0xFF;
    gdt[idx].access      = access;
    gdt[idx].flags_limit = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[idx].base_high   = (base >> 24) & 0xFF;
}

static void set_tss_gate(int idx, uintptr_t base, uint32_t limit)
{
    struct gdt_tss_entry *e = (struct gdt_tss_entry *)&gdt[idx];
    e->limit_low   = limit & 0xFFFF;
    e->base_0_15   = base & 0xFFFF;
    e->base_16_23  = (base >> 16) & 0xFF;
    e->access      = 0x89;
    e->flags_limit = ((limit >> 16) & 0x0F);
    e->base_24_31  = (base >> 24) & 0xFF;
    e->base_32_63  = (uint32_t)(base >> 32);
    e->reserved    = 0;
}

void init_gdt(void)
{
    set_gate(0, 0, 0, 0, 0);                     // null seg       
    set_gate(1, 0, 0xFFFFF, 0x9A, 0xA0);         // kernel code
    set_gate(2, 0, 0xFFFFF, 0x92, 0xC0);         // kernel data
    set_gate(3, 0, 0xFFFFF, 0xF2, 0xC0);         // user data
    set_gate(4, 0, 0xFFFFF, 0xFA, 0xA0);         // user code

    set_tss_gate(5, (uintptr_t)&tss, sizeof(tss) - 1);

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;
    gdt_flush((uint64_t)&gdt_ptr);
}

void init_tss(uintptr_t rsp0)
{
    Sys_Info("Initing Kernel TSS \n");
    memset(&tss, 0, sizeof(tss));
    tss.rsp0      = rsp0;
    tss.iomap_base = sizeof(tss);
    __asm__ volatile("ltr %0" :: "r"((uint16_t)(TSS_GDT_INDEX << 3)));
    Sys_Success("Kernel TSS Inited \n");
}

void setTss_sp(uintptr_t rsp0) { tss.rsp0 = rsp0; }