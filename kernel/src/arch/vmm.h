#pragma once
#include <stdint.h>
#include <stddef.h>

#define PTE_PRESENT   (1ULL << 0)
#define PTE_WRITABLE  (1ULL << 1)
#define PTE_USER      (1ULL << 2)
#define PTE_HUGE      (1ULL << 7)
#define PTE_GLOBAL    (1ULL << 8)
#define PTE_NX        (1ULL << 63)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PAGE_SIZE_4K  0x1000ULL
#define PAGE_SIZE_2M  0x200000ULL
#define PAGE_SIZE_1G  0x40000000ULL

#define PML4_INDEX(a) (((a) >> 39) & 0x1FF)
#define PDPT_INDEX(a) (((a) >> 30) & 0x1FF)
#define PD_INDEX(a)   (((a) >> 21) & 0x1FF)
#define PT_INDEX(a)   (((a) >> 12) & 0x1FF)

#define HHDM_VBASE 0xFFFF800000000000ULL

#ifndef HHDM_SIZE_GB
#define HHDM_SIZE_GB 4096
#endif

#define PHYS_2_HHDM(phys) (phys + HHDM_VBASE)

_Static_assert(HHDM_SIZE_GB > 0, "HHDM_SIZE_GB must be positive");
_Static_assert(HHDM_SIZE_GB <= 255ULL * 512, "HHDM_SIZE_GB would overlap kernel's PML4 entry");

#define KERNEL_VMA_BASE  0xFFFF900000000000ULL
#define KERNEL_VMA_SIZE  (512ULL * 1024 * 1024 * 1024)


#define KERNEL_VBASE 0xFFFFFFFF80000000ULL


typedef uintptr_t page_index;

#define ADDR_TO_PAGE(addr) ((page_index)((addr) / PAGE_SIZE_4K))
#define PAGE_TO_ADDR(pg)   ((uintptr_t)(pg) * PAGE_SIZE_4K)

extern uintptr_t kernel_pagedir_phys;
extern volatile uintptr_t hhdm_offset;


typedef struct vma_region {
    uintptr_t base;
    size_t pages;
    struct vma_region* next;
} vma_region_t;



void hhdm_init();

void map_4k(uint64_t *pml4, uintptr_t va, uintptr_t pa, uint64_t flags);
void map_2m(uint64_t *pml4, uintptr_t va, uintptr_t pa, uint64_t flags);
void map_1g(uint64_t *pml4, uintptr_t va, uintptr_t pa, uint64_t flags);

void vmm_map_page(uintptr_t vpage, uintptr_t ppage, uint64_t flags);
void vmm_unmap_page(uintptr_t vpage);

void vmm_kvma_init();

uintptr_t kvma_alloc(size_t count);
void kvma_free(uintptr_t base, size_t count);

uintptr_t vmm_virt_to_phys(uintptr_t vaddr);