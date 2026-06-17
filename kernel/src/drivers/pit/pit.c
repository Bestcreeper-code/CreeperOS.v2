#include "pit.h"
#include "Debug/Logger.h"
#include "asm/ams.h"
#include "drivers/drivers.h"
#include <stdint.h>

#define PIT_COMMAND   0x43
#define PIT_CHANNEL0  0x40
#define PIT_FREQUENCY 1000// Hz

uint64_t timer_ticks_ms = 0;

REGISTER_DRIVER_CORE(pit, pit_init);

int pit_init() {
    Sys_log("initializing PIT\n");
    timer_ticks_ms = 0;

    uint16_t divisor = 1193180 / PIT_FREQUENCY;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    Sys_Success("PIT initialized.\n");
    return 0;
}

// irq0
void timer_irq() {
    timer_ticks_ms++;
}

void sleep(uint64_t ms) {
    uint64_t target = timer_ticks_ms + ms;
    while (timer_ticks_ms < target)
        __asm__ volatile ("hlt");
}

uint64_t rdtsc() {
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}