#include "pit.h"
#include "debug/Logger.h"
#include "arch/interrupts.h"
#include "asm/ams.h"
#include "defines/compiler_defs.h"
#include "defines/err_codes.h"
#include "drivers/drivers.h"
#include "memory/memory.h"
#include "timer/timers.h"
#include <stddef.h>
#include <stdint.h>

#define PIT_COMMAND   0x43
#define PIT_CHANNEL0  0x40
#define PIT_FREQUENCY 1000// Hz

#define HARDCODED_PIT_INTERRUPT_VECTOR 32
volatile uint64_t pit_timer_ticks_ms = 0;

timer_registery_id _pit_timer_device_id;




void pit_isr_entry();

uint64_t pit_gettime() {
    return pit_timer_ticks_ms;
}
void pit_reset_int(timer_dev*) {
    outb(0x20, 0x20);//EOI
}


int pit_init() {
    Sys_log("initializing PIT\n");
    pit_timer_ticks_ms = 0;
    
    timer_dev* dev = kmalloc(sizeof(timer_dev));
    if(!dev) return -E_NOMEM;
    dev->name = "PIT";
    dev->type = TIMER_DEV_PERIODIC;
    
    dev->freq = 1000;

    dev->enable = NULL;
    dev->disable = NULL;
    dev->settime = NULL;

    dev->gettime_us = pit_gettime;
    dev->reset_interrupt = pit_reset_int;
    
    dev->vector = HARDCODED_PIT_INTERRUPT_VECTOR;
    
    
    setup_interrupt_vector(HARDCODED_PIT_INTERRUPT_VECTOR, pit_isr_entry, IRQ_FLAG_INTERRUPT);
    
    uint16_t divisor = 1193180 / PIT_FREQUENCY;
    
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
    
    
    
    _pit_timer_device_id = timer_register(dev, TIMER_ROLE_SCHED | TIMER_ROLE_SLEEP, 1);
     
    if(_pit_timer_device_id < 0){
        Sys_Error("issue initing PIT: (%d)",_pit_timer_device_id);
        return _pit_timer_device_id;
    }
    
    Sys_Success("PIT initialized.\n");
    return 0;
}
REGISTER_DRIVER_DEV(pit, pit_init);



uint64_t rdtsc() {
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}