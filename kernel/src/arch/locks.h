#pragma once

#include <stddef.h>
#include <stdint.h>


//bullshit code just for stuff to work
void acquire_lock(size_t* addr, uint8_t bit);
int try_acquire_lock(size_t* addr, uint8_t bit);

void release_lock(size_t* addr, uint8_t bit);