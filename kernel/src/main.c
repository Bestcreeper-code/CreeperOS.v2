#include "arch/arch.h"
#include "arch/vmm.h"
#include "arch/x86_64/scheduler/scheduler.h"
#include "asm/asm.h"
#include "debug/Logger.h"
#include "defines/types.h"
#include "drivers/drivers.h"
#include "Flanterm/src/flanterm_backends/fb.h"
#include "Flanterm/src/flanterm.h"
#include "initrd_parse/initrd.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include "mm/vmm_arch.h"
#include "requests.h"
#include "timer/time.h"
#include "vfs/fs.h"
#include "vfs/ramfile.h"
#include "vfs/sysfs.h"
#include "vfs/vfs.h"

#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    k_ramfile_mkdir("/tmp", 0666);
    
    log_queue_init();
    linked_pcb* klogger_pcb = ktask_start(_log_manager_thread, "klogger");
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
    
    // for(;;);
    sti();
    // enable_scheduler();
    physptr_t test_pml4 = pmm_alloc_pages_zeroed(1);
    Sys_Debug("pml4=%p\n", (void*)test_pml4);


    dentry* testbin_file = kpath_lookup(root_dentry->inode, "/initrd/test.bin");
    Sys_Debug("dentry=%p\n", testbin_file);

    if (!testbin_file) {
        Sys_Error("lookup failed\n");
    }

    Sys_Debug("inode=%p\n", testbin_file->inode);

    if (!testbin_file->inode) {
        Sys_Error("inode null\n");
    }

    Sys_Debug("i_fop=%p\n", testbin_file->inode->i_fop);

    physptr_t buffer = pmm_alloc_zeroed();
    Sys_Debug("buffer=%p\n", (void*)buffer);

    if (!buffer) {
        Sys_Error("buffer alloc failed\n");
    }

    file* test_fil = kmalloc(sizeof(file));
    Sys_Debug("file=%p\n", test_fil);

    if (!test_fil) {
        Sys_Error("file alloc failed\n");
    }

    int open_ret = testbin_file->inode->i_fop->open(testbin_file->inode, test_fil);
    Sys_Debug("open_ret=%d\n", open_ret);

    Sys_Debug("after open: f_ops=%p f_inode=%p private=%p\n",
            test_fil->f_ops,
            test_fil->f_inode,
            test_fil->private_data);

    if (open_ret != 0) {
        Sys_Error("open failed\n");
    }

    loff_t off = 0;

    Sys_Debug("before read: f_ops=%p read=%p inode_size=%llu\n",
            test_fil->f_ops,
            test_fil->f_ops ? test_fil->f_ops->read : NULL,
            testbin_file->inode->i_size);

    if (!test_fil->f_ops) {
        Sys_Error("f_ops NULL after open\n");
    }

    if (!test_fil->f_inode) {
        Sys_Error("f_inode NULL after open\n");
    }

    ssize_t r = test_fil->f_ops->read(
        test_fil,
        PHYS_2_HHDM(buffer),
        testbin_file->inode->i_size,
        &off
    );

    Sys_Debug("read_ret=%ld off=%ld\n", r, (long)off);

    if (r == (ssize_t)test_fil->f_inode->i_size) {
        Sys_Debug("read OK\n");
        map_4k(PHYS_2_HHDM(test_pml4), 0x100000, buffer, PTE_USER|PTE_WRITABLE|PTE_PRESENT);
    } else {
        Sys_Error("read mismatch\n");
    }
    
    void dump_userspace_mappings(uint64_t* pml4);
    dump_userspace_mappings(PHYS_2_HHDM(test_pml4));
    map_4k(PHYS_2_HHDM(test_pml4), 0x100000, buffer, PTE_USER | PTE_WRITABLE | PTE_LOCAL_OWNED);
    us_task_start((void*)0x100000,"test uspace", test_pml4);
   
    
    // Sys_Warning("Kernel reached halt????\n");
    hcf();
}


void dump_userspace_mappings(uint64_t* pml4) {
    Sys_Debug("---- userspace mapping dump (PML4[0..255]) ----\n");
    Sys_Debug("pml4 virt is %p\n",pml4);

    for (int pml4_i = 0; pml4_i < 256; pml4_i++) {
        uint64_t pml4_e = pml4[pml4_i];
        if (!(pml4_e & PTE_PRESENT)) continue;

        uint64_t* pdpt = (uint64_t*)((pml4_e & PTE_ADDR_MASK) + hhdm_offset);

        for (int pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            uint64_t pdpt_e = pdpt[pdpt_i];
            if (!(pdpt_e & PTE_PRESENT)) continue;

            uintptr_t va_pdpt = ((uintptr_t)pml4_i << 39) | ((uintptr_t)pdpt_i << 30);

            if (pdpt_e & PTE_HUGE) {
                Sys_Debug("1G  VA=%p -> PA=%p  flags: %s %s %s %s\n",
                    (void*)va_pdpt,
                    (void*)(pdpt_e & PTE_ADDR_MASK),
                    (pdpt_e & PTE_WRITABLE) ? "W" : "-",
                    (pdpt_e & PTE_USER)     ? "U" : "-",
                    (pdpt_e & PTE_NX)       ? "NX" : "X",
                    (pdpt_e & PTE_LOCAL_OWNED)  ? "O" : "NO");
                continue;
            }

            uint64_t* pd = (uint64_t*)((pdpt_e & PTE_ADDR_MASK) + hhdm_offset);

            for (int pd_i = 0; pd_i < 512; pd_i++) {
                uint64_t pd_e = pd[pd_i];
                if (!(pd_e & PTE_PRESENT)) continue;

                uintptr_t va_pd = va_pdpt | ((uintptr_t)pd_i << 21);

                if (pd_e & PTE_HUGE) {
                    Sys_Debug("2M  VA=%p -> PA=%p  flags: %s %s %s %s\n",
                        (void*)va_pd,
                        (void*)(pd_e & PTE_ADDR_MASK),
                        (pd_e & PTE_WRITABLE) ? "W" : "-",
                        (pd_e & PTE_USER)     ? "U" : "-",
                        (pd_e & PTE_NX)       ? "NX" : "X",
                        (pd_e & PTE_LOCAL_OWNED)  ? "O" : "NO");
                    continue;
                }

                uint64_t* pt = (uint64_t*)((pd_e & PTE_ADDR_MASK) + hhdm_offset);

                for (int pt_i = 0; pt_i < 512; pt_i++) {
                    uint64_t pt_e = pt[pt_i];
                    if (!(pt_e & PTE_PRESENT)) continue;

                    uintptr_t va = va_pd | ((uintptr_t)pt_i << 12);

                    Sys_Debug("4K  VA=%p -> PA=%p  flags: %s %s %s %s\n",
                        (void*)va,
                        (void*)(pt_e & PTE_ADDR_MASK),
                        (pt_e & PTE_WRITABLE) ? "W" : "-",
                        (pt_e & PTE_USER)     ? "U" : "-",
                        (pt_e & PTE_NX)       ? "NX" : "X",
                        (pt_e & PTE_LOCAL_OWNED)  ? "O" : "NO");
                }
            }
        }
    }

    Sys_Debug("---- end mapping dump ----\n");
}