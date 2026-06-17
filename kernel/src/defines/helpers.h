#pragma once 
#include "defines/types.h"
#include <stddef.h>


size_t bitmap_alloc_1_first(char *bitmap, size_t nbytes);
size_t wbitmap_alloc_1_first(char *bitmap, size_t nbytes);

static inline void bitmap_set(char *bitmap, size_t idx, int state) {
    size_t byte = idx / 8;
    size_t bit  = idx % 8;

    if (state)
        bitmap[byte] |=  (1U << bit);
    else
        bitmap[byte] &= ~(1U << bit);
}