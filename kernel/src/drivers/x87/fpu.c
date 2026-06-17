#include "fpu.h"

#include "Debug/Logger.h"
#include "arch/x86_64/cpu/cpu.h"
#include "drivers/drivers.h"





int x87_fpu_init() {
    if (cpu_features & CPU_FEAT_FPU) {
        if (x87_fpu_try_config()) {
            return 0;
        }
    }
    else {
        Sys_Error("x87 FPU not detected\n");
    }
    return -1;
}
REGISTER_DRIVER_DEV(x87_fpu, x87_fpu_init); 