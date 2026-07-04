#include "helpers.h"



size_t bitmap_alloc_1_first(char* bitmap, size_t nbytes)
{
    for (size_t i = 0; i < nbytes; i++) {

        unsigned char byte = (unsigned char)bitmap[i];

        if (byte == 0x00)
            continue;

        unsigned bit = __builtin_ctz(byte);

        size_t idx = i * 8 + bit;

        bitmap_set(bitmap, idx, 0);
        return idx;
    }

    return (size_t)-1;
}

size_t wbitmap_alloc_1_first(size_t* bitmap, size_t nbytes)
{
    size_t nwords = nbytes / sizeof(size_t);
    size_t *words = (size_t *)bitmap;

    for (size_t i = 0; i < nwords; i++) {

        size_t w = words[i];

        if (w == 0)
            continue;

        size_t bit = __builtin_ctzll(w);

        size_t idx = i * (sizeof(size_t) * 8) + bit;

        bitmap_set((char*)bitmap, idx, 0);
        return idx;
    }

    return (size_t)-1;
}