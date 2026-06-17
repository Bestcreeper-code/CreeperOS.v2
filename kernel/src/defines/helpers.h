#pragma once 
#include "defines/types.h"
#include <stddef.h>


size_t bitmap_alloc_1_first(char *bitmap, size_t nbytes);
ssize_t wbitmap_alloc_1_first(char *bitmap, size_t nbytes);

inline static void bitmap_zero_bit(char *bitmap, size_t pos) {
    size_t byte = pos / 8;
    size_t bit  = pos % 8;
    bitmap[byte] &= ~(1U << bit);
    
}