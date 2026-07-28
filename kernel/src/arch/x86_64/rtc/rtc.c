#include "rtc.h"
#include "asm/asm.h"
#include "timer/time.h"
#include <stdbool.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static inline uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

static inline uint8_t read_rtc_register(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

bool rtc_read_time(rtc_time_t* time) {
    if (!time) return false;

    outb(CMOS_ADDRESS, 0x0A);
    while (inb(CMOS_DATA) & 0x80);

    uint8_t sec  = read_rtc_register(0x00);
    uint8_t min  = read_rtc_register(0x02);
    uint8_t hour = read_rtc_register(0x04);
    uint8_t day  = read_rtc_register(0x07);
    uint8_t mon  = read_rtc_register(0x08);
    uint8_t year = read_rtc_register(0x09);
    uint8_t reg_b = read_rtc_register(0x0B);

    bool is_bcd = !(reg_b & 0x04);
    if (is_bcd) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        year = bcd_to_bin(year);
    }

    time->second = sec;
    time->minute = min;
    time->hour   = hour;
    time->day    = day;
    time->month  = mon;
    time->year   = 2000 + year; // assuming year is 00–99

    return true;
}



static inline bool is_leap_year(uint32_t year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) ||
           (year % 400 == 0);
}

static uint64_t boot_unix_time = 0;
void rtc_init()
{
    rtc_time_t time;
    rtc_read_time(&time);

    static const uint8_t days_before_month[] =
    {
        0,   // Jan
        31,  // Feb
        59,  // Mar
        90,  // Apr
        120, // May
        151, // Jun
        181, // Jul
        212, // Aug
        243, // Sep
        273, // Oct
        304, // Nov
        334  // Dec
    };

    uint64_t days = 0;

    for (uint32_t y = 1970; y < time.year; y++)
        days += is_leap_year(y) ? 366 : 365;

    days += days_before_month[time.month - 1];

    if (time.month > 2 && is_leap_year(time.year))
        days++;

    days += time.day - 1;

    boot_unix_time =
        (days * 86400ULL +
        (uint64_t)time.hour * 3600ULL +
        (uint64_t)time.minute * 60ULL +
        (uint64_t)time.second) - timer_get_us()/1000000;
}


uint64_t get_unixtime()
{
    return boot_unix_time + timer_get_us()/1000000;
}