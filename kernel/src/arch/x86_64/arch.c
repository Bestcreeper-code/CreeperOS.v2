#include "arch/arch.h"
#include "Debug/Logger.h"
#include "Debug/panic.h"
#include "arch/vmm.h"
#include "asm/ams.h"
#include "defines/compiler_defs.h"
#include "interrupts/pic.h"
#include "cpu/cpu.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include <stdint.h>


GCC_ATTR((noreturn))
void arch_init(){
    Sys_log("Entering arch init\n");
    register_cpu_features();

    pic_remap();

    init_gdt();
    idt_init();
    register_cpu_exceptions();

    //mem
    pmm_init();
    
    hhdm_init();
    
    vmm_kvma_init();


    Sys_Success("Paging set up\n");
    
    Sys_log("Setting up Kernel Stack.\n");
    
    uintptr_t allocated_stack_pages = page_kalloc(KERNEL_STACK_PAGE_AMOUNT+1, PTE_NX | PTE_WRITABLE);//+1 to besafe
    uintptr_t allocated_stack_top = (allocated_stack_pages + (PAGE_SIZE_4K*KERNEL_STACK_PAGE_AMOUNT));

    if (!allocated_stack_pages) {
        panic("Couldn't allocate kernel stack :(\n");
    }
    Sys_Success("kernel stack allocated\n");
    
    __asm__ volatile(
        "movq %0, %%rsp\n"
        :
        : "r"(allocated_stack_top)
    );
    Sys_Success("kernel stack switched to %lx\n",sp_get());
    init_tss(allocated_stack_top);
    
    
    
    void kmain();
    kmain();
    
}