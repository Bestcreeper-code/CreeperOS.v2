#pragma once
#include "asm/asm.h"
#include "time.h"
#include <stdbool.h>
#include <stdint.h>



#define TIMER_ROLE_SYSTEM (1 << 0)
#define TIMER_ROLE_SLEEP (1 << 1)
#define TIMER_ROLE_SCHED (1 << 2)  

typedef int8_t timer_registery_id ;

enum timer_dev_type {
    TIMER_DEV_ONESHOT           = 0,
    TIMER_DEV_PERIODIC          = 1,
    TIMER_DEV_TIMESTAMP_COUNTER = 2,
    TIMER_DEV_RTC               = 3,
};

typedef struct timer_dev {
    const char* name;
    uint32_t freq;
    uint8_t type;
    uint8_t vector;
    uint32_t flags;

    void (*enable)(struct timer_dev* );
    void (*disable)(struct timer_dev* );
    uint64_t (*gettime_us)(struct timer_dev* );
    int (*settime)(struct timer_dev* , uint64_t);
    void (*reset_interrupt)(struct timer_dev* );
} timer_dev;

int8_t timer_register(timer_dev* dev, uint32_t roles, int priority);
timer_dev* timer_get_system_time_dev();
bool common_timer_dispatcher(timer_registery_id id, register_t sp);