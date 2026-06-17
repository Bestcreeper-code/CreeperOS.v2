#include "arch/rtc.h"

#include "asm/ams.h"
#include "drivers/drivers.h"
#include <stdint.h>
#include <stdbool.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71


static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

static uint8_t read_rtc_register(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}


bool rtc_read_time(rtc_time_t *time) {
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
    time->year   = 2000 + year;

    return true;
}



static const int days_in_month[] = {
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
};

bool is_leap_year(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

uint32_t rtc_to_unix_timestamp(rtc_time_t *rtc) {
    uint32_t days = 0;

    for (int y = 1970; y < rtc->year; y++)
        days += is_leap_year(y) ? 366 : 365;

    for (int m = 1; m < rtc->month; m++) {
        days += days_in_month[m - 1];
        if (m == 2 && is_leap_year(rtc->year))
            days += 1;
    }

    days += (rtc->day - 1);

    uint32_t seconds = (uint32_t)((uint64_t)days * 86400ULL);
    seconds += rtc->hour   * 3600U;
    seconds += rtc->minute * 60U;
    seconds += rtc->second;

    return seconds;
}

void rtc_add_seconds(rtc_time_t *t, uint32_t seconds) {
    if (!t) return;

    
    t->second += seconds % 60;
    if (t->second >= 60) { t->second -= 60; t->minute++; }

    seconds /= 60;
    t->minute += seconds % 60;
    if (t->minute >= 60) { t->minute -= 60; t->hour++; }

    
    seconds /= 60;
    t->hour += seconds % 24;
    if (t->hour >= 24) { t->hour -= 24; t->day++; }

    
    seconds /= 24;
    t->day += seconds;

    while (1) {
        int dim = days_in_month[t->month - 1];
        if (t->month == 2 && is_leap_year(t->year))
            dim = 29;

        if (t->day <= dim) break;

        t->day -= dim;
        t->month++;
        if (t->month > 12) {
            t->month = 1;
            t->year++;
        }
    }
}