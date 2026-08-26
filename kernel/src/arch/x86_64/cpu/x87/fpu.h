#pragma once
#include <stdbool.h>
#include <stdint.h>

extern uint8_t fpu_enabled;
bool x87_fpu_try_config();
int fpu_init();