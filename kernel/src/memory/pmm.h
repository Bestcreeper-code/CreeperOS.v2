#pragma once

#include <stdint.h>
#include <stddef.h>

#define PMM_PAGE_SIZE 0x1000ULL

typedef uintptr_t physptr_t;

void pmm_init();

uintptr_t pmm_alloc();
uintptr_t pmm_alloc_zeroed();
void pmm_free(uintptr_t phys);

uintptr_t pmm_alloc_pages(size_t count);
uintptr_t pmm_alloc_pages_zeroed(size_t count);
void pmm_free_pages(uintptr_t phys, size_t count);

size_t pmm_get_total_pages();
size_t pmm_get_free_pages();