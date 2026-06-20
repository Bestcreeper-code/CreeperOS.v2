#include "timers.h"
#include "arch/x86_64/scheduler/scheduler.h"
#include "debug/Logger.h"
#include "asm/ams.h"
#include "defines/err_codes.h"
#include <stddef.h>
#include <stdint.h>



#define TIMER_IDX_BITS 5
#define TIMER_TYPE_BITS 2

#define TIMER_IDX_MASK 0x1F        // 5 
#define TIMER_TYPE_MASK 0x03        // 2 

#define TIMER_TYPE_SHIFT TIMER_IDX_BITS
#define TIMER_ERROR_BIT 0x80        

typedef struct {
    timer_dev* dev;
    int priority;
} timer_slot;

static timer_slot slot_system_time = { NULL, -1 };
static timer_slot slot_sleep = { NULL, -1 };
static timer_slot slot_sched = { NULL, -1 };

static timer_dev* devices_by_type[4][32];
static uint8_t devices_by_type_count[4];

volatile uint64_t ticks_ms = 0;
volatile uint64_t ticks_us = 0;

static void try_promote(timer_slot* slot, timer_dev* dev, int priority, const char* role_name) {
    if (priority > slot->priority) {
        slot->dev = dev;
        slot->priority = priority;
        Sys_Info("timer: %s slot -> %s (prio %d)\n", role_name, dev->name, priority);
    }
}

timer_registery_id timer_register(timer_dev* dev, uint32_t roles, int priority) {
    if (dev ==NULL) return -E_INVAL;

    uint8_t type = dev->type & TIMER_TYPE_MASK;

    if (devices_by_type_count[type] >= 32) return -E_NOSPC;

    uint8_t idx = devices_by_type_count[type]++;
    devices_by_type[type][idx] = dev;

    if (roles & TIMER_ROLE_SYSTEM) try_promote(&slot_system_time, dev, priority, "system");

    if (roles & TIMER_ROLE_SLEEP) try_promote(&slot_sleep, dev, priority, "sleep");

    if (roles & TIMER_ROLE_SCHED) try_promote(&slot_sched, dev, priority, "sched");

    if (dev->enable) dev->enable(dev);

    uint8_t encoded =
        ((type & TIMER_TYPE_MASK) << TIMER_TYPE_SHIFT) |
        (idx & TIMER_IDX_MASK);

    return (timer_registery_id)encoded;
}

timer_dev* timer_get_system_time_dev() {
    return slot_system_time.dev;
}

bool common_timer_dispatcher(timer_registery_id id) {
    int8_t sid = id;

    
    if (sid < 0)
        return false;

    uint8_t uid = (uint8_t)sid;

    uint8_t type = (uid >> TIMER_TYPE_SHIFT) & TIMER_TYPE_MASK;
    uint8_t idx  = uid & TIMER_IDX_MASK;

    if (type >= 4)
        return false;

    if (idx >= devices_by_type_count[type])
        return false;

    timer_dev* firing = devices_by_type[type][idx];Sys_log("%lu",firing->freq);
    if (!firing)
        return false;

    if (firing->reset_interrupt)
        firing->reset_interrupt(firing);

    if (firing == slot_sleep.dev) {
        Sys_log("%lu",firing->freq);
        if(!firing->freq){
            return false;
        }
        ticks_us += (1000000ULL / firing->freq);
        ticks_ms = ticks_us / 1000;
        Sys_log("%lx", ticks_us);
    }

    if (firing == slot_sched.dev)
        _sched_next_process();

    return true;
}