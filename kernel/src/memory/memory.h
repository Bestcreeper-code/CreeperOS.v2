#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Debug/Logger.h"
#include "config.h"
// #include "kernel_data.h"


#define MULTIBOOT_MMAP_FREE_MEMORY  1
#define MULTIBOOT_MMAP_RESERVED     2

#define MAX_FREE_REGIONS 128

typedef struct {
    uintptr_t base_addr;
    size_t length;
} __attribute__((packed)) free_region_t;

typedef struct {
    int free_region_count;
    free_region_t free_regions[MAX_FREE_REGIONS];
} __attribute__((packed)) free_region_map_t;


extern volatile size_t ram_amount;
extern free_region_map_t* k_mmap;

size_t get_pter_size(void* pter) __attribute__((nonnull (1)));



void force_alloc(uintptr_t address, size_t size);
void force_free(uintptr_t address, size_t size);

void print_free_regions();

uintptr_t get_pter_size(void *pter);

void kfree_impl(void *_Memory); __attribute__((nonnull (1)));
void *kmalloc_impl(size_t size); __attribute__ ((malloc, malloc (kfree_impl, 1)));

void *krealloc_impl(void *ptr, size_t size); 

void *aligned_malloc(size_t size, size_t alignment);
void aligned_free(void *ptr);



// alloc VA + PA, map them, return the VA
uintptr_t page_kalloc(size_t count, uint64_t flags);

void page_kfree(uintptr_t va, size_t count);

#define kmalloc(size) kmalloc_impl(size)//;Sys_log("\n");
#define kfree(ptr) kfree_impl(ptr)//;Sys_log("\n");
#define krealloc             krealloc_impl
