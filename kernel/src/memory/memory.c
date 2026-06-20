#include "memory/memory.h"

#include "arch/vmm.h"
#include "drivers/drivers.h"
#include "memory/pmm.h"
#include "memops.h"




static free_region_map_t region_map;
free_region_map_t *k_mmap = &region_map;

static inline free_region_map_t *get_free_region_map() { return k_mmap; }



void force_alloc(uintptr_t address, size_t size) {
    free_region_map_t *k_mmap = get_free_region_map();
    uintptr_t end       = address + size;
    size_t    page_size = PMM_PAGE_SIZE;

    for (uintptr_t page_addr = address & ~(page_size - 1);
         page_addr < end;
         page_addr += page_size) {

        uintptr_t page_start = page_addr;
        uintptr_t page_end   = page_addr + page_size;

        if (address > page_start)
            force_free(page_start, address - page_start);

        if (end < page_end)
            force_free(end, page_end - end);
    }

    uintptr_t lock_end = end;
    for (int i = 0; i < k_mmap->free_region_count; i++) {
        free_region_t *region     = &k_mmap->free_regions[i];
        uintptr_t      region_end = region->base_addr + region->length;

        if (lock_end <= region->base_addr || address >= region_end) continue;

        if (address <= region->base_addr && lock_end >= region_end) {
            for (int j = i; j < k_mmap->free_region_count - 1; j++)
                k_mmap->free_regions[j] = k_mmap->free_regions[j + 1];
            k_mmap->free_region_count--;
            i--;
            continue;
        }

        if (address <= region->base_addr && lock_end < region_end) {
            region->length    = region_end - lock_end;
            region->base_addr = lock_end;
            continue;
        }

        if (address > region->base_addr && lock_end >= region_end) {
            region->length = address - region->base_addr;
            continue;
        }

        if (address > region->base_addr && lock_end < region_end) {
            size_t    first_len   = address - region->base_addr;
            uintptr_t second_base = lock_end;
            size_t    second_len  = region_end - lock_end;
            region->length = first_len;

            if (k_mmap->free_region_count < MAX_FREE_REGIONS) {
                for (int j = k_mmap->free_region_count; j > i + 1; j--)
                    k_mmap->free_regions[j] = k_mmap->free_regions[j - 1];
                k_mmap->free_regions[i + 1].base_addr = second_base;
                k_mmap->free_regions[i + 1].length    = second_len;
                k_mmap->free_region_count++;
            }
            continue;
        }
    }
}

void force_free(uintptr_t address, size_t size) {
    free_region_map_t *k_mmap = get_free_region_map();

    uintptr_t new_start = address & ~0xFFFU;
    uintptr_t new_end   = (address + size + 0xFFFU) & ~0xFFFU;

    if (new_end <= 0x10000) return;
    if (new_start < 0x10000) new_start = 0x10000;

    for (int i = 0; i < k_mmap->free_region_count; i++) {
        free_region_t *region      = &k_mmap->free_regions[i];
        uintptr_t      region_start = region->base_addr;
        uintptr_t      region_end   = region->base_addr + region->length;

        if (new_end >= region_start && new_start <= region_end) {
            uintptr_t merged_start = (new_start < region_start) ? new_start : region_start;
            uintptr_t merged_end   = (new_end   > region_end)   ? new_end   : region_end;
            region->base_addr = merged_start;
            region->length    = merged_end - merged_start;

            for (int j = 0; j < k_mmap->free_region_count; j++) {
                if (j == i) continue;
                free_region_t *other       = &k_mmap->free_regions[j];
                uintptr_t      other_start = other->base_addr;
                uintptr_t      other_end   = other->base_addr + other->length;

                if (region->base_addr <= other_end &&
                    region->base_addr + region->length >= other_start) {

                    uintptr_t merge_start = (region->base_addr < other_start)
                                          ? region->base_addr : other_start;
                    uintptr_t merge_end   = ((region->base_addr + region->length) > other_end)
                                          ? (region->base_addr + region->length) : other_end;
                    region->base_addr = merge_start;
                    region->length    = merge_end - merge_start;

                    for (int k = j; k < k_mmap->free_region_count - 1; k++)
                        k_mmap->free_regions[k] = k_mmap->free_regions[k + 1];
                    k_mmap->free_regions[k_mmap->free_region_count - 1].base_addr = 0;
                    k_mmap->free_regions[k_mmap->free_region_count - 1].length    = 0;
                    k_mmap->free_region_count--;
                    if (j < i) i--;
                    j--;
                }
            }
            return;
        }
    }

    if (k_mmap->free_region_count < MAX_FREE_REGIONS) {
        k_mmap->free_regions[k_mmap->free_region_count].base_addr = new_start;
        k_mmap->free_regions[k_mmap->free_region_count].length    = new_end - new_start;
        k_mmap->free_region_count++;
    }
}


#define KERNEL_HEAP_SEED_PAGES 4


int init_allocator() {
    for (int i = 0; i < KERNEL_HEAP_SEED_PAGES; i++) {
        uintptr_t pa = pmm_alloc_zeroed();
        if (!pa) {
            Sys_log("k_allocator: pmm_alloc_zeroed() failed on seed page %d\n", i);
            return -1;
        }

        uintptr_t va = kvma_alloc(1);
        if (!va) {
            pmm_free(pa);
            Sys_log("k_allocator: kvma_alloc() failed on seed page %d\n", i);
            return -1;
        }

        uint64_t *pml4 = (uint64_t *)PHYS_2_HHDM(kernel_pagedir_phys);
        map_4k(pml4, va, pa, PTE_PRESENT | PTE_WRITABLE);

        if (k_mmap->free_region_count < MAX_FREE_REGIONS) {
            k_mmap->free_regions[k_mmap->free_region_count].base_addr = va;
            k_mmap->free_regions[k_mmap->free_region_count].length    = PMM_PAGE_SIZE;
            k_mmap->free_region_count++;
        }
    }

    Sys_log("k_allocator: seeded %d page(s) into heap region map:\n",
            k_mmap->free_region_count);
    for (int i = 0; i < k_mmap->free_region_count; i++) {
        Sys_log("  [%d] va=0x%lx  len=0x%lx\n",
                i,
                k_mmap->free_regions[i].base_addr,
                k_mmap->free_regions[i].length);
    }

    return 0;
}
REGISTER_DRIVER_CORE(k_allocator, init_allocator);


free_region_t *FirstRegionOfSizeOrMore(size_t size) {
    free_region_map_t *k_mmap       = get_free_region_map();
    size_t             requestedSize = (size + sizeof(uintptr_t) + 7) & ~7U;

    for (int i = 0; i < k_mmap->free_region_count; i++) {
        if (k_mmap->free_regions[i].length == 0) continue;

        uintptr_t currBase = k_mmap->free_regions[i].base_addr;
        size_t    currSize = k_mmap->free_regions[i].length;

        bool merged[MAX_FREE_REGIONS] = {false};
        merged[i] = true;

        bool changed;
        do {
            changed = false;
            for (int j = 0; j < k_mmap->free_region_count; j++) {
                if (merged[j] || k_mmap->free_regions[j].length == 0) continue;
                if (k_mmap->free_regions[j].base_addr == currBase + currSize) {
                    currSize += k_mmap->free_regions[j].length;
                    merged[j] = true;
                    changed    = true;
                }
            }
        } while (changed);

        if (currSize >= requestedSize) {
            for (int j = 0; j < k_mmap->free_region_count; j++) {
                if (merged[j] && j != i)
                    memset(&k_mmap->free_regions[j], 0, sizeof(free_region_t));
            }
            k_mmap->free_regions[i].length = currSize;
            return &k_mmap->free_regions[i];
        }
    }

    return NULL;
}

void print_free_regions() {
    free_region_map_t *k_mmap = get_free_region_map();
    Sys_log("Free regions:\n");
    for (int i = 0; i < MAX_FREE_REGIONS; i++) {
        if (k_mmap->free_regions[i].length > 0) {
            Sys_log("[%d] base: 0x%lx, size: %zu\n",
                    i,
                    k_mmap->free_regions[i].base_addr,
                    k_mmap->free_regions[i].length);
        }
    }
}

uintptr_t get_pter_size(void *pter) {
    uint8_t   *raw     = (uint8_t *)pter;
    uintptr_t *sizeaddr = (uintptr_t *)(raw - sizeof(uintptr_t));
    return *sizeaddr;
}



void *kmalloc_impl(size_t size) {
#if MEM_DEBUG
    Sys_log("[MEM_DBG] kmalloc(%zu)\n", size);
#endif
    if (size == 0) return NULL;

    free_region_map_t *k_mmap = get_free_region_map();
    if (!k_mmap) return NULL;

    if (size > SIZE_MAX - sizeof(uintptr_t)) return NULL;
    size_t full_size = (size + sizeof(uintptr_t) + 7) & ~7U;

    free_region_t *region = FirstRegionOfSizeOrMore(full_size);

    if (!region || region->base_addr <= 0xFFFF) {
        
        size_t pages_needed = (full_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

        uintptr_t pa = pmm_alloc_pages_zeroed(pages_needed);
        if (!pa) {
            Sys_Error("kmalloc: pmm_alloc_pages_zeroed(%zu) failed\n", pages_needed);
            return NULL;
        }

        uintptr_t va = kvma_alloc(pages_needed);
        if (!va) {
            pmm_free_pages(pa, pages_needed);
            Sys_Error("kmalloc: kvma_alloc(%zu) failed\n", pages_needed);
            return NULL;
        }

        uint64_t *pml4 = (uint64_t *)PHYS_2_HHDM(kernel_pagedir_phys);
        for (size_t i = 0; i < pages_needed; i++)
            map_4k(pml4,
                   va + i * PMM_PAGE_SIZE,
                   pa + i * PMM_PAGE_SIZE,
                   PTE_PRESENT | PTE_WRITABLE);

        force_free(va, pages_needed * PMM_PAGE_SIZE);
        region = FirstRegionOfSizeOrMore(full_size);
    }

    if (!region || region->base_addr <= 0xFFFF) {
        Sys_Error("kmalloc: still no usable region after expansion\n");
        return NULL;
    }

    uintptr_t *header = (uintptr_t *)region->base_addr;
    *header = (uintptr_t)size;

    region->base_addr += full_size;
    region->length    -= full_size;

    if (region->length == 0) {
        int idx = (int)(region - k_mmap->free_regions);
        for (int i = idx; i < k_mmap->free_region_count - 1; i++)
            k_mmap->free_regions[i] = k_mmap->free_regions[i + 1];
        k_mmap->free_region_count--;
    }

#if MEM_DEBUG
    Sys_log("[MEM_DBG] kmalloc -> %p (%zu bytes)\n", header + 1, full_size);
#endif
    return (void *)(header + 1);
}



void kfree_impl(void *_Memory) {
#if MEM_DEBUG
    Sys_log("[MEM_DBG] kfree(%p)\n", _Memory);
#endif
    free_region_map_t *k_mmap = get_free_region_map();

    uintptr_t  address = (uintptr_t)_Memory;
    uintptr_t *sizeptr = (uintptr_t *)(address - sizeof(uintptr_t));
    size_t     size    = *sizeptr + sizeof(uintptr_t);
    size = (size + 7) & ~7U;

    for (int i = 0; i < MAX_FREE_REGIONS; i++) {
        if (k_mmap->free_regions[i].length == 0) {
            k_mmap->free_regions[i].base_addr = address - sizeof(uintptr_t);
            k_mmap->free_regions[i].length    = size;
#if MEM_DEBUG
            Sys_log("[MEM_DBG] kfree: returned %zu bytes at %p\n", size, _Memory);
#endif
            break;
        }
    }

#ifndef FREE_MERGES_MMAP_BLOCKS
    for (int i = 0; i < MAX_FREE_REGIONS - 1; i++) {
        for (int j = i + 1; j < MAX_FREE_REGIONS; j++) {
            if (k_mmap->free_regions[j].length == 0) continue;
            if (k_mmap->free_regions[i].length == 0 ||
                k_mmap->free_regions[j].base_addr < k_mmap->free_regions[i].base_addr) {
                free_region_t tmp          = k_mmap->free_regions[i];
                k_mmap->free_regions[i]    = k_mmap->free_regions[j];
                k_mmap->free_regions[j]    = tmp;
            }
        }
    }

    for (int i = 0; i < MAX_FREE_REGIONS - 1; i++) {
        if (k_mmap->free_regions[i].length == 0) continue;
        for (int j = i + 1; j < MAX_FREE_REGIONS; j++) {
            if (k_mmap->free_regions[j].length == 0) continue;
            uintptr_t end_i = k_mmap->free_regions[i].base_addr +
                              k_mmap->free_regions[i].length;
            if (end_i == k_mmap->free_regions[j].base_addr) {
                k_mmap->free_regions[i].length += k_mmap->free_regions[j].length;
                for (int k = j; k < MAX_FREE_REGIONS - 1; k++)
                    k_mmap->free_regions[k] = k_mmap->free_regions[k + 1];
                k_mmap->free_regions[MAX_FREE_REGIONS - 1].base_addr = 0;
                k_mmap->free_regions[MAX_FREE_REGIONS - 1].length    = 0;
                j--;
            }
        }
    }
#endif


    k_mmap->free_region_count = 0;
    for (int i = 0; i < MAX_FREE_REGIONS; i++) {
        if (k_mmap->free_regions[i].length > 0)
            k_mmap->free_region_count++;
    }
}


void *krealloc_impl(void *ptr, size_t size) {
#if MEM_DEBUG
    Sys_log("[MEM_DBG] krealloc(%p, %zu)\n", ptr, size);
#endif
    if (size == 0) { kfree(ptr); return NULL; }
    if (!ptr)       return kmalloc(size);

    void  *new_ptr   = kmalloc(size);
    if (!new_ptr) return NULL;

    size_t old_len   = get_pter_size(ptr);
    size_t copy_size = (size < old_len) ? size : old_len;

    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);
    return new_ptr;
}

void *aligned_malloc(size_t size, size_t alignment) {
    if ((alignment & (alignment - 1)) != 0) {
        Sys_log("aligned_malloc: alignment must be a power of two\n");
        return NULL;
    }

    size_t extra = alignment - 1 + sizeof(uintptr_t);
    void  *raw   = kmalloc_impl(size + extra);
    if (!raw) return NULL;

    uintptr_t raw_addr = (uintptr_t)raw;
    uintptr_t aligned_addr = (raw_addr + sizeof(uintptr_t) + alignment - 1)
                             & ~(alignment - 1);

    ((uintptr_t *)aligned_addr)[-1] = raw_addr;
    return (void *)aligned_addr;
}

void aligned_free(void *ptr) {
    uintptr_t  aligned_addr = (uintptr_t)ptr;
    void      *raw          = (void *)((uintptr_t *)aligned_addr)[-1];
    kfree_impl(raw);
}



uintptr_t page_kalloc(size_t count, uint64_t flags) {
    uintptr_t pa  = pmm_alloc_pages_zeroed(count);
    uintptr_t va  = kvma_alloc(count);

    uint64_t *pml4 = (uint64_t *)PHYS_2_HHDM(kernel_pagedir_phys);
    for (size_t i = 0; i < count; i++)
        map_4k(pml4,
               va + i * PMM_PAGE_SIZE,
               pa + i * PMM_PAGE_SIZE,
               flags);
    return va;
}

void page_kfree(uintptr_t va, size_t count) {
    uintptr_t pa = vmm_virt_to_phys(va);

    for (size_t i = 0; i < count; i++)
        vmm_unmap_page(ADDR_TO_PAGE(va + i * PMM_PAGE_SIZE));

    pmm_free_pages(pa, count);
    kvma_free(va, count);
}