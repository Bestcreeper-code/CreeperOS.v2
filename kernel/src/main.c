#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "arch/arch.h"
#include "asm/ams.h"
#include "drivers/drivers.h"
#include "initrd_parse/initrd.h"
#include "string/string.h"
#include "Debug/Logger.h"
#include "memory/pmm.h"
#include "memops.h"
#include "arch/vmm.h"
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


    
    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        framebuffer->address, framebuffer->width, framebuffer->height, 
        framebuffer->pitch,
        framebuffer->red_mask_size, framebuffer->red_mask_shift,
        framebuffer->green_mask_size, framebuffer->green_mask_shift,
        framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0,
        0
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

void kmain() {
    sti();
    
    core_init();
    initrd_init();
    // #error fzjifhzoeuhfouzehofuo initrf #pf
    dev_init();
    sysfs_init();
    tree(root_dentry, 0);
    
    // fs_init();
    // late_init();
    // We're done, just hang...
    Sys_Warning("Kernel reached halt\n");
    hcf();
}