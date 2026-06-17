#include <stdint.h>


void timer_register(uint64_t (*get_ticks_ms)(), int priority);

uint64_t timer_get_ms();

void sleep_ms(uint64_t ms);