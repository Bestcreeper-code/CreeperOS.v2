#pragma once
#include "asm/asm.h"
#include "time.h"
#include <stdbool.h>
#include <stdint.h>



typedef int clockid_t;

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7
#define CLOCK_REALTIME_ALARM 8
#define CLOCK_BOOTTIME_ALARM 9
#define CLOCK_TAI 11


#define TIMER_ROLE_SYSTEM_TIME (1 << 0)
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
    uint64_t freq;
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