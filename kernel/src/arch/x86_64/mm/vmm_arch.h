#pragma once 

#define PTE_PRESENT   (1ULL << 0)
#define PTE_WRITABLE  (1ULL << 1)
#define PTE_USER      (1ULL << 2)
#define PTE_PWT       (1ULL << 3)
#define PTE_CACHE_DIS (1ULL << 4)
#define PTE_DIRTY     (1ULL << 6)
#define PTE_HUGE      (1ULL << 7)
#define PTE_GLOBAL    (1ULL << 8)

#define PTE_AVAIL1    (1ULL << 9)
#define PTE_AVAIL2    (1ULL << 10)
#define PTE_AVAIL3    (1ULL << 11)

#define PTE_NX        (1ULL << 63)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define PTE_FLAGS_MASK  (~PTE_ADDR_MASK)

#define PTE_LOCAL_OWNED PTE_AVAIL1

#define PT_ENTRIES           512