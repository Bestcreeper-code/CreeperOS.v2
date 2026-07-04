#include "interrupts.h"
#include "interrupts/pic.h"
#include <stddef.h>
#include <stdint.h>


void (*_current_eoi)(uint8_t vector) = pic_send_eoi;