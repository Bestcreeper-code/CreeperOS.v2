#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
} rtc_time_t;


bool rtc_read_time(rtc_time_t *time);

uint32_t rtc_to_unix_timestamp(rtc_time_t *rtc);

void rtc_add_seconds(rtc_time_t *t, uint32_t seconds);

bool is_leap_year(int year);