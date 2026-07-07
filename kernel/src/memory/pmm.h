#pragma once

#include <stdint.h>
#include <stddef.h>

#define PMM_PAGE_SIZE 0x1000ULL

typedef uintptr_t physptr_t;

void pmm_init();

physptr_t pmm_alloc();
physptr_t pmm_alloc_zeroed();
void pmm_free(physptr_t phys);

physptr_t pmm_alloc_pages(size_t count);
physptr_t pmm_alloc_pages_zeroed(size_t count);
void pmm_free_pages(physptr_t phys, size_t count);

size_t pmm_get_total_pages();
size_t pmm_get_free_pages();