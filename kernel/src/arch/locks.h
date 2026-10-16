#pragma once

#include <stddef.h>
#include <stdint.h>


//bullshit code just for stuff to work
// void acquire_lock(size_t* addr, uint8_t bit);
// int try_acquire_lock(size_t* addr, uint8_t bit);

void release_lock(size_t* addr, uint8_t bit);

static inline unsigned long try_lock_irqsave(volatile uint32_t *l) {
    unsigned long f;
    asm volatile("pushfq; pop %0; cli" : "=r"(f) :: "memory");

    if (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) {
        asm volatile("push %0; popfq" :: "r"(f) : "memory", "cc");
        return 0;
    }

    return f;
}

static inline unsigned long spin_lock_irqsave(volatile uint32_t *l){
    unsigned long f; asm volatile("pushfq; pop %0; cli":"=r"(f)::"memory");
    while(__atomic_exchange_n(l,1,__ATOMIC_ACQUIRE)) __builtin_ia32_pause();
    return f;
}

static inline void spin_unlock_irqrestore(volatile uint32_t *l, unsigned long f){
    __atomic_store_n(l,0,__ATOMIC_RELEASE);
    if(f & (1<<9)) asm volatile("sti":::"memory");
}