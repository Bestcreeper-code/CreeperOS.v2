#pragma once
#include <stdint.h>


extern uint64_t pit_timer_ticks_ms;

int pit_init();

void timer_irq();

void sleep(uint64_t ms);

uint64_t rdtsc();