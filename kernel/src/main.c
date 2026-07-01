#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "arch/arch.h"
#include "arch/x86_64/scheduler/scheduler.h"
#include "asm/asm.h"
#include "defines/types.h"
#include "drivers/drivers.h"
#include "initrd_parse/initrd.h"
#include "memory/memory.h"
#include "mm/vmm_arch.h"
#include "string/string.h"
#include "debug/Logger.h"
#include "memory/pmm.h"
#include "memops.h"
#include "arch/vmm.h"
#include "timer/time.h"
#include "vfs/fs.h"
#include "vfs/sysfs.h"
#include "printf/printf.h"
#include "requests.h"
#include "Flanterm/src/flanterm.h"
#include "Flanterm/src/flanterm_backends/fb.h"
#include "vfs/vfs.h"

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

struct flanterm_context* ft_ctx;



void _kstart() {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    
    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    struct limine_flanterm_fb_init_params* fl_init = flanterm_request.response->entries[0];
    
    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        framebuffer->address, framebuffer->width, framebuffer->height, 
        framebuffer->pitch,
        framebuffer->red_mask_size, framebuffer->red_mask_shift,
        framebuffer->green_mask_size, framebuffer->green_mask_shift,
        framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
        fl_init->canvas,
        fl_init->ansi_colours, fl_init->ansi_bright_colours,
        &fl_init->default_bg, &fl_init->default_fg,
        &fl_init->default_bg_bright, &fl_init->default_fg_bright,
        fl_init->font, 
        fl_init->font_width, fl_init->font_height,
        fl_init->font_spacing, 
        fl_init->font_scale_x, fl_init->font_scale_y,
        fl_init->margin, fl_init->rotation
    );
    serial_init();
        
    Sys_Success("fb init \r\n");
    
    
    
    
    Sys_Debug("Debug\n");
    Sys_Info("Info\n");
    Sys_Success("Success\n");
    Sys_Warning("Warning\n");
    Sys_Error("Error\n");
    Sys_Fatal("Fatal\n");

    arch_init();//calls kmain at the end
}

volatile int counter = 0;
void loop(){
    int id=counter++;
    for(int i = 0; i < 10000; i++){
        // char* d =kmalloc(80);
        // sprintf(d,"\e[31m0000000\e[0m\n");
        // kfree(d);
        // printf_("%s",d);
        Sys_log("\e[33mkthread %d",id);
        Sys_Success("\n");
        sleep_ms(20);
    }
    _scheduler_current_process->exit_code=-999999;
    kill_ktask(_scheduler_current_process);
    _yield();
}

void kmain() {
    cli();
    
    core_init();
    sysfs_init();
    // fs_init();
    scheduler_init();
    



    initrd_init();

    flanterm_clear(ft_ctx, 0);
    
    // for(int i = 0; i < 22; i++){
    //     hlt();
    // }
        
    dev_init();

    
    for(int i = 0; i < 70; i++){
        Sys_Info("\n", i, ktask_start(loop, "test-loop")->pid);
    }
    
    
    flanterm_clear(ft_ctx, 0);

    log_queue_init();
    Linked_PCB_t* klogger_pcb = ktask_start(_log_manager_thread, "klogger");
    if(!klogger_pcb){
        Sys_Error("couldn't init logger thread");
        for(;;);
    }else {
        Sys_Success("klogger started as pid:%d",klogger_pcb->pid);
        // for(;;);
    }

    flanterm_clear(ft_ctx, 2);
    // flanterm_full_refresh(ft_ctx);
    // flanterm_flush(ft_ctx);
    // for(;;);
    sti();
    // flanterm_full_refresh(ft_ctx);
    // late_init();
    
    for(;;);
    Sys_Warning("Kernel reached halt????\n");
    // hcf();
}
