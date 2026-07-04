#pragma once

#include <stdint.h>
void pic_remap();
void block_pic();

void pic_send_eoi(uint8_t vector);