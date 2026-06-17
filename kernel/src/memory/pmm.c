#include "pmm.h"
#include "Debug/Logger.h"
#include "Debug/panic.h"
#include "memops.h"
#include <stdint.h>
#include <stddef.h>
#include <limine.h>
extern struct limine_hhdm_request hhdm_request;
extern struct limine_memmap_request memmap_request;



static inline void *phys_to_virt(uintptr_t phys) {
    return (void *)(phys + hhdm_request.response->offset);
}

static uint8_t* bitmap;
static size_t bitmap_bits;
static size_t bitmap_next = 0;

static size_t total_pages;
static size_t free_pages;

static inline void bitmap_set(size_t bit) {
    bitmap[bit >> 3] |= (1u << (bit & 7));
}

static inline void bitmap_clear(size_t bit) {
    bitmap[bit >> 3] &= ~(1u << (bit & 7));
}

static inline int bitmap_test(size_t bit) {
    return (bitmap[bit >> 3] >> (bit & 7)) & 1;
}

static size_t bitmap_find_clear() {
    for (size_t pass = 0; pass < 2; pass++) {
        size_t start = (pass == 0) ? bitmap_next : 0;
        size_t end   = (pass == 0) ? bitmap_bits : bitmap_next;
        for (size_t i = start; i < end; i++) {
            if (!bitmap_test(i)) {
                bitmap_next = i + 1;
                if (bitmap_next >= bitmap_bits) bitmap_next = 0;
                return i;
            }
        }
    }
    return SIZE_MAX;
}

static size_t bitmap_find_clear_range(size_t count) {
    size_t run = 0, start = 0;
    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (run == 0) start = i;
            if (++run == count) return start;
        } else {
            run = 0;
        }
    }
    return SIZE_MAX;
}




void pmm_init() {
    struct limine_memmap_response *mm = memmap_request.response;
    uintptr_t highest_addr = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        uintptr_t end = (uintptr_t)(e->base + e->length);
        if (e->type == LIMINE_MEMMAP_USABLE && end > highest_addr)
        highest_addr = end;
    }
    total_pages = (size_t)(highest_addr / PMM_PAGE_SIZE);
    bitmap_bits = total_pages;
    bitmap_next = 0;
    size_t bitmap_size = (total_pages + 7) / 8;
    uintptr_t bitmap_phys = 0;
    int found = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_size) {
            bitmap_phys = (uintptr_t)e->base;
            found = 1;
            break;
        }
    }

    if (!found) panic("no usable region large enough for the bitmap");

    bitmap = phys_to_virt(bitmap_phys);
    memset(bitmap, 0xFF, bitmap_size);
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) continue;
        
        size_t first = (size_t)(e->base / PMM_PAGE_SIZE);
        size_t count = (size_t)(e->length / PMM_PAGE_SIZE);
        for (size_t p = 0; p < count; p++) {
            bitmap_clear(first + p);
            free_pages++;
        }
    }

    size_t bitmap_first = (size_t)(bitmap_phys / PMM_PAGE_SIZE);
    size_t bitmap_pages = (bitmap_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    for (size_t p = 0; p < bitmap_pages; p++) {
        if (!bitmap_test(bitmap_first + p)) {
            bitmap_set(bitmap_first + p);
            free_pages--;
        }
    }
}

uintptr_t pmm_alloc() {
    size_t bit = bitmap_find_clear();
    if (bit == SIZE_MAX) panic("pmm: out of memory");

    bitmap_set(bit);
    free_pages--;
    return (uintptr_t)bit * PMM_PAGE_SIZE;
}

uintptr_t pmm_alloc_zeroed() {
    uintptr_t phys = pmm_alloc();
    memset(phys_to_virt(phys), 0, PMM_PAGE_SIZE);
    return phys;
}

void pmm_free(uintptr_t phys) {
    size_t bit = (size_t)(phys / PMM_PAGE_SIZE);
    if (bitmap_test(bit)) {
        bitmap_clear(bit);
        free_pages++;
    }
}


uintptr_t pmm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc();  // fast path

    size_t bit = bitmap_find_clear_range(count);
    if (bit == SIZE_MAX) panic("pmm: out of memory");

    for (size_t i = 0; i < count; i++)
        bitmap_set(bit + i);
    free_pages -= count;

    return (uintptr_t)bit * PMM_PAGE_SIZE;
}

uintptr_t pmm_alloc_pages_zeroed(size_t count) {
    uintptr_t phys = pmm_alloc_pages(count);
    memset(phys_to_virt(phys), 0, count * PMM_PAGE_SIZE);
    return phys;
}

void pmm_free_pages(uintptr_t phys, size_t count) {
    size_t bit = phys / PMM_PAGE_SIZE;
    for (size_t i = 0; i < count; i++)
        bitmap_clear(bit + i);
    free_pages += count;
}

size_t pmm_get_total_pages() {
    return total_pages;
}

size_t pmm_get_free_pages() {
    return free_pages;
}