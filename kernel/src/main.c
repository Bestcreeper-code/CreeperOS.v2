#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "arch/arch.h"
#include "arch/x86_64/scheduler/scheduler.h"
#include "asm/asm.h"
#include "defines/types.h"
#include "drivers/drivers.h"
#include "drivers/pit/pit.h"
#include "initrd_parse/initrd.h"
#include "memory/memory.h"
#include "mm/vmm_arch.h"
#include "string/format.h"
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
void loop(void) {
    static volatile uint32_t rng = 0x12345678;

    #define RAND() ({ \
        rng ^= rng << 13; \
        rng ^= rng >> 17; \
        rng ^= rng << 5; \
        rng; \
    })

    static const uint32_t colors[16] = {
        0x000000, 0xAA0000, 0x00AA00, 0xAAAA00,
        0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
        0x555555, 0xFF5555, 0x55FF55, 0xFFFF55,
        0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF
    };

    int x = RAND() % (framebuffer_request.response->framebuffers[0]->width - 3);
    int y = RAND() % (framebuffer_request.response->framebuffers[0]->height - 3);

    int dx = (RAND() % 3) - 1;
    int dy = (RAND() % 3) - 1;

    if (dx == 0 && dy == 0)
        dx = 1;

    uint32_t color = colors[(RAND() % 15) + 1];

    for (int ticks = 0; ticks < 100000; ticks++) {
        for (int yy = 0; yy < 10; yy++) {
            uint32_t *row = (uint32_t *)((uint8_t *)framebuffer_request.response->framebuffers[0]->address +
                                         (y + yy) * framebuffer_request.response->framebuffers[0]->pitch);
            for (int xx = 0; xx < 10; xx++)
                row[x + xx] = 0x000000;
        }

        if ((RAND() & 31) == 0) {
            dx = (RAND() % 3) - 1;
            dy = (RAND() % 3) - 1;
            if (dx == 0 && dy == 0)
                dx = 1;
        }

        x += dx;
        y += dy;

        if (x < 0) { x = 0; dx = -dx; }
        if (y < 0) { y = 0; dy = -dy; }

        if (x > (int)framebuffer_request.response->framebuffers[0]->width - 3) {
            x = framebuffer_request.response->framebuffers[0]->width - 3;
            dx = -dx;
        }

        if (y > (int)framebuffer_request.response->framebuffers[0]->height - 3) {
            y = framebuffer_request.response->framebuffers[0]->height - 3;
            dy = -dy;
        }

        for (int yy = 0; yy < 10; yy++) {
            uint32_t *row = (uint32_t *)((uint8_t *)framebuffer_request.response->framebuffers[0]->address +
                                         (y + yy) * framebuffer_request.response->framebuffers[0]->pitch);
            for (int xx = 0; xx < 10; xx++)
                row[x + xx] = color;
        }

        sleep_ms(5);
    }

    // erase the particle before exiting
    for (int yy = 0; yy < 3; yy++) {
        uint32_t *row = (uint32_t *)((uint8_t *)framebuffer_request.response->framebuffers[0]->address +
                                     (y + yy) * framebuffer_request.response->framebuffers[0]->pitch);
        for (int xx = 0; xx < 3; xx++)
            row[x + xx] = 0x000000;
    }

    _scheduler_current_process->exit_code = 0;
    kill_ktask(_scheduler_current_process);
    _yield();

    #undef RAND
}

void kmain() {
    cli();
    
    core_init();
    sysfs_init();
    
    // fs_init();
    scheduler_init();
    
    
    log_queue_init();
    Linked_PCB_t* klogger_pcb = ktask_start(_log_manager_thread, "klogger");
    if(!klogger_pcb){
        Sys_Error("couldn't init logger thread\n");
        for(;;);
    }else {
        Sys_Success("klogger started as pid:%d\n",klogger_pcb->pid);
        // for(;;);
    }
    
    // for(int i = 0; i < 22; i++){
    //     hlt();
    // }


    
    
    sti();
    initrd_init();
        
    dev_init();
    // {  
    //     for(int j = 0; j < 200; j++){
    //         ktask_start(loop, "test-loop");

    //         char buff[100];
    //         byte_nb_simplify(pmm_get_free_pages() * PAGE_SIZE_4K, buff, 100);
    //         Sys_log("%s\n", buff);
    //     }
    //     _log_all_processes();
    // }
    
    us_task_start(init_drivers,"test uspace",pmm_alloc());

    Sys_Warning("Kernel reached halt????\n");
    for(;;);
    hcf();
}
