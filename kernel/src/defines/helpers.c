#include "helpers.h"



size_t bitmap_alloc_1_first(char *bitmap, size_t nbytes) {
    for (size_t i = 0; i < nbytes; i++) {
        unsigned char byte = bitmap[i];
        if (byte != 0xFF) {
            unsigned char inv = ~byte;
            unsigned char bit = __builtin_ffs(inv) - 1;

            bitmap_set(bitmap, i * 8 + bit, 1);
            return i * 8 + bit;
        }
    }
    return -1;
}

size_t wbitmap_alloc_1_first(char *bitmap, size_t nbytes) {
    size_t nwords = (nbytes + sizeof(size_t) - 1) / sizeof(size_t);
    size_t *words = (size_t*)bitmap;

    for (size_t i = 0; i < nwords; i++) {
        if (words[i] != ~(size_t)0) {
            size_t inv = ~words[i];
            unsigned bit = __builtin_ffsll(inv) - 1;

            size_t idx = i * sizeof(size_t) * 8 + bit;
            bitmap_set(bitmap, idx, 1);

            return idx;
        }
    }
    return -1;
}