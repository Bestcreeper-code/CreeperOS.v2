#include "fpu.h"

#include "debug/Logger.h"
#include "arch/x86_64/cpu/cpu.h"
#include "drivers/drivers.h"



uint8_t fpu_enabled = 0;

int fpu_init(void)
{
    if (!x87_fpu_try_config()) {
        Sys_Error("Failed to initialize x87/SSE FPU\n");
        return -1;
    }
    fpu_enabled=1;
    return 0;
}
