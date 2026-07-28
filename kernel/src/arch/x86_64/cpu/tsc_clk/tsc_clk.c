




#include "debug/Logger.h"
#include "defines/err_codes.h"
#include "drivers/pit/pit.h"
#include "memory/memory.h"
#include "timer/timers.h"

#include <stddef.h>
#include <cpuid.h>



static size_t tsc_freq;

static inline bool has_invariant_tsc()
{
    uint32_t eax, ebx, ecx, edx;

    __cpuid(0x80000007, eax, ebx, ecx, edx);

    return (edx & (1 << 8)) != 0;//tsc invariant bit
}

uint64_t get_tsc_freq(void)
{
    uint32_t eax, ebx, ecx, edx;

    __cpuid(0x15, eax, ebx, ecx, edx);

    if (eax == 0 || ebx == 0 || ecx == 0)
        return 0;

    return ((uint64_t)ecx * ebx) / eax;
}

uint64_t tsc_gettime_us(struct timer_dev*)
{
    return (__uint128_t)rdtsc() * 1000000ULL / tsc_freq;
}

int tsc_clk_init() {

    
    if(!has_invariant_tsc()) {
        Sys_Error("TSC is not invariant\n");
        return -1;
    }
    
    tsc_freq = get_tsc_freq();
    if(!tsc_freq){
        Sys_Error("TSC freq not detected\n");
        return -1;
    }
    
    
    static timer_dev tsc_tim_dev;//no malloc at that point in boot

    tsc_tim_dev.name = "tsc clock";
    tsc_tim_dev.freq = tsc_freq;
    tsc_tim_dev.gettime_us = tsc_gettime_us;
    tsc_tim_dev.type = TIMER_DEV_TIMESTAMP_COUNTER;
    //vector/irq etc not needed since only sys time


    timer_register(
        &tsc_tim_dev,
        TIMER_ROLE_SYSTEM_TIME,
        0x7FFFFFFF//if it exists and is fine for use.. why not just use it since faster/simple
    );
    return 0;
}