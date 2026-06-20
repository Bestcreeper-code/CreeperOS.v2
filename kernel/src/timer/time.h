#pragma once
#include <stdint.h>



uint64_t timer_get_ms();
uint64_t timer_get_us();
void sleep_ms(uint64_t ms);
void sleep_us(uint64_t us);