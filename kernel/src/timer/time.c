
#include "time.h"
#include "Debug/Logger.h"
#include "asm/ams.h"
#include <stddef.h>
#include <stdint.h>

static uint64_t (*curr_timer_ms)() = NULL;
static int active_priority = -1;

void timer_register(uint64_t (*get_ticks_ms)(), int priority) {
    if (priority > active_priority) {
        curr_timer_ms = get_ticks_ms;
        active_priority = priority;
        Sys_Info("kernel timer switched to %p (prio:%u)\n", get_ticks_ms, priority);
    }
}

uint64_t timer_get_ms() {
    if (!curr_timer_ms) return 0;
    return curr_timer_ms();
}

void sleep_ms(uint64_t ms) {
    uint64_t target = timer_get_ms() + ms;
    while (timer_get_ms() < target) {
        hlt();
    }
}