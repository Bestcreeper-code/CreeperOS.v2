#include "time.h"
#include "scheduler/scheduler.h"
#include "timers.h"
#include "asm/asm.h"
#include <stdint.h>

extern volatile uint64_t ticks_ms;
extern volatile uint64_t ticks_us;



uint64_t timer_get_ms() {
    timer_dev *sys = timer_get_system_time_dev();
    if (sys) return sys->gettime_us(sys) / 1000000ULL;
    return ticks_ms;
}

uint64_t timer_get_us() {
    timer_dev *sys = timer_get_system_time_dev();
    if (sys) return sys->gettime_us(sys) / 1000ULL;
    return ticks_us;
}

void sleep_ms(uint64_t ms) {
    uint64_t target = timer_get_ms() + ms;
    while (timer_get_ms() < target)
        _yield();
}

void sleep_us(uint64_t us) {
    uint64_t target = timer_get_us() + us;
    while (timer_get_us() < target)
        _yield();
}