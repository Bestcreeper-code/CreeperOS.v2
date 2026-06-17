#include "arch/vmm.h"
#include "Debug/Logger.h"
#include "Debug/panic.h"
#include "asm/ams.h"
#include "defines/container_of.h"
#include "memory/pmm.h"
#include "printf/printf.h"
#include "requests.h"

#include <stdint.h>
#include <stddef.h>

#include <cpuid.h>
#include <limine.h>

extern char _kernel_start[];
extern char _kernel_end[];


volatile uintptr_t hhdm_offset = 0;
uintptr_t kernel_pml4_phys = 0;


static inline uint64_t *phys_to_boot_virt(uintptr_t phys) {
    return (uint64_t *)(phys + hhdm_request.response->offset);
}

static inline int cpu_has_1gb_pages() {
    uint32_t eax, ebx, ecx, edx;
    __cpuid(0x80000001, eax, ebx, ecx, edx);
    return (edx >> 26) & 1;
}

static uint64_t *table_get_or_create(uint64_t *table, size_t idx) {
    if (!(table[idx] & PTE_PRESENT)) {//<<<
        uintptr_t phys = pmm_alloc_zeroed();
        table[idx] = (phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }

    uintptr_t offset = hhdm_offset ? hhdm_offset : hhdm_request.response->offset;
    return (uint64_t *)((table[idx] & PTE_ADDR_MASK) + offset);
}

void map_4k(uint64_t *pml4, uintptr_t va, uintptr_t pa, uint64_t flags) {
    uint64_t *pdpt = table_get_or_create(pml4, PML4_INDEX(va));//<<
    uint64_t *pd   = table_get_or_create(pdpt, PDPT_INDEX(va));
    uint64_t *pt   = table_get_or_create(pd,   PD_INDEX(va));
    pt[PT_INDEX(va)] = (pa & PTE_ADDR_MASK) | flags | PTE_PRESENT;
}

void map_2m(uint64_t *pml4, uintptr_t va, uintptr_t pa, uint64_t flags) {
    uint64_t *pdpt = table_get_or_create(pml4, PML4_INDEX(va));
    uint64_t *pd   = table_get_or_create(pdpt, PDPT_INDEX(va));
    pd[PD_INDEX(va)] = (pa & PTE_ADDR_MASK) | flags | PTE_HUGE | PTE_PRESENT;
}

void map_1g(uint64_t *pml4, uintptr_t va, uintptr_t pa, uint64_t flags) {
    uint64_t *pdpt = table_get_or_create(pml4, PML4_INDEX(va));
    pdpt[PDPT_INDEX(va)] = (pa & PTE_ADDR_MASK) | flags | PTE_HUGE | PTE_PRESENT;
}

static void hhdm_check_coverage() {
    struct limine_memmap_response *mm = memmap_request.response;
    uintptr_t top = 0;

    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        uintptr_t end = (uintptr_t)(e->base + e->length);
        Sys_Info("limine_memmap_entries[%d]" "\t" "base:%lx" "\t" "length:%lx" "\t" "type:%ld\n",i,e->base,e->length,e->type);
        if (end > top) top = end;
    }

    if (top > (uintptr_t)HHDM_SIZE_GB * PAGE_SIZE_1G) {
        char buffer[128];
        sprintf(buffer,"HHDM_SIZE_GB(%lx) too small for installed RAM(%lx)",HHDM_SIZE_GB*PAGE_SIZE_1G, top);
        panic(buffer);
    }
}

static void hhdm_map_physical_memory(uint64_t *pml4) {
    uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_NX;
    int huge1g = cpu_has_1gb_pages();

    for (size_t gb = 0; gb < HHDM_SIZE_GB; gb++) {
        uintptr_t pa = (uintptr_t)gb * PAGE_SIZE_1G;
        uintptr_t va = HHDM_VBASE + pa;

        if (huge1g) {
            map_1g(pml4, va, pa, flags);
            continue;
        }

        for (uintptr_t off = 0; off < PAGE_SIZE_1G; off += PAGE_SIZE_2M)
            map_2m(pml4, va + off, pa + off, flags);
    }
}

static void hhdm_map_kernel(uint64_t *pml4) {
    uintptr_t kphys = (uintptr_t)kaddr_request.response->physical_base;
    uintptr_t kvirt = (uintptr_t)kaddr_request.response->virtual_base;
    uintptr_t ksize = (uintptr_t)_kernel_end - (uintptr_t)_kernel_start;
    ksize = (ksize + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);

    for (uintptr_t off = 0; off < ksize; off += PAGE_SIZE_4K)
        map_4k(pml4, kvirt + off, kphys + off, PTE_PRESENT | PTE_WRITABLE);
}

void hhdm_init() {
    hhdm_check_coverage();

    uintptr_t pml4_phys = pmm_alloc_zeroed();
    uint64_t *pml4 = phys_to_boot_virt(pml4_phys);

    hhdm_map_physical_memory(pml4);
    hhdm_map_kernel(pml4);

    hhdm_offset = HHDM_VBASE;
    kernel_pml4_phys = pml4_phys;
    cr3_set(pml4_phys);
}

uintptr_t vmm_virt_to_phys(uintptr_t vaddr) {
    uintptr_t cr3 = cr3_get();

    uint64_t *pml4 = (uint64_t *)(cr3 + hhdm_offset);
    uint64_t pdpt_e = pml4[PML4_INDEX(vaddr)];
    if (!(pdpt_e & PTE_PRESENT)) return UINTPTR_MAX;

    uint64_t *pdpt = (uint64_t *)((pdpt_e & PTE_ADDR_MASK) + hhdm_offset);
    uint64_t pd_e = pdpt[PDPT_INDEX(vaddr)];
    if (!(pd_e & PTE_PRESENT)) return UINTPTR_MAX;
    if (pd_e & PTE_HUGE)
        return (pd_e & PTE_ADDR_MASK) | (vaddr & (PAGE_SIZE_1G - 1));

    uint64_t *pd = (uint64_t *)((pd_e & PTE_ADDR_MASK) + hhdm_offset);
    uint64_t pt_e = pd[PD_INDEX(vaddr)];
    if (!(pt_e & PTE_PRESENT)) return UINTPTR_MAX;
    if (pt_e & PTE_HUGE)
        return (pt_e & PTE_ADDR_MASK) | (vaddr & (PAGE_SIZE_2M - 1));

    uint64_t *pt = (uint64_t *)((pt_e & PTE_ADDR_MASK) + hhdm_offset);
    uint64_t page_e = pt[PT_INDEX(vaddr)];
    if (!(page_e & PTE_PRESENT)) return UINTPTR_MAX;

    return (page_e & PTE_ADDR_MASK) | (vaddr & (PAGE_SIZE_4K - 1));
}

static uint64_t *get_current_pml4() {
    uintptr_t cr3 = cr3_get();
    return (uint64_t *)(cr3 + hhdm_offset);
}

void vmm_map_page(uintptr_t va, uintptr_t pa, uint64_t flags) {
    map_4k(get_current_pml4(), va, pa, flags);
    invlpg(va);
}

static int table_is_empty(uint64_t *table) {
    for (int i = 0; i < 512; i++)
        if (table[i]) return 0;
    return 1;
}

void vmm_unmap_page(uintptr_t va) {
    
    uint64_t *pml4 = get_current_pml4();

    uint64_t pdpt_e = pml4[PML4_INDEX(va)];
    if (!(pdpt_e & PTE_PRESENT)) return;
    uint64_t *pdpt = (uint64_t *)((pdpt_e & PTE_ADDR_MASK) + hhdm_offset);

    uint64_t pd_e = pdpt[PDPT_INDEX(va)];
    if (!(pd_e & PTE_PRESENT) || (pd_e & PTE_HUGE)) return;
    uint64_t *pd = (uint64_t *)((pd_e & PTE_ADDR_MASK) + hhdm_offset);

    uint64_t pt_e = pd[PD_INDEX(va)];
    if (!(pt_e & PTE_PRESENT) || (pt_e & PTE_HUGE)) return;
    uint64_t *pt = (uint64_t *)((pt_e & PTE_ADDR_MASK) + hhdm_offset);

    pt[PT_INDEX(va)] = 0;
    invlpg(va);

    if (table_is_empty(pt)) {
        uintptr_t pt_phys = pt_e & PTE_ADDR_MASK;
        pd[PD_INDEX(va)] = 0;
        pmm_free(pt_phys);

        if (table_is_empty(pd)) {
            uintptr_t pd_phys = pd_e & PTE_ADDR_MASK;
            pdpt[PDPT_INDEX(va)] = 0;
            pmm_free(pd_phys);

            if (table_is_empty(pdpt)) {
                uintptr_t pdpt_phys = pdpt_e & PTE_ADDR_MASK;
                pml4[PML4_INDEX(va)] = 0;
                pmm_free(pdpt_phys);
            }
        }
    }
}







static vma_region_t  vma_pool[512];
static vma_region_t *vma_free_list = NULL;

static vma_region_t *kvma_node_alloc() {
    for (size_t i = 0; i < 512; i++) {
        if (vma_pool[i].pages == 0) return &vma_pool[i];
    }
    panic("vmm: vma_pool exhausted");
}


void vmm_kvma_init() {
    vma_region_t *r = kvma_node_alloc();
    r->base  = KERNEL_VMA_BASE;
    r->pages = KERNEL_VMA_SIZE / PMM_PAGE_SIZE;
    r->next  = NULL;
    vma_free_list = r;
}


uintptr_t kvma_alloc(size_t count) {
    vma_region_t **prev = &vma_free_list;
    vma_region_t  *cur  = vma_free_list;

    while (cur) {
        if (cur->pages >= count) {
            uintptr_t base = cur->base;
            cur->base  += count * PMM_PAGE_SIZE;
            cur->pages -= count;
            if (cur->pages == 0) {
                *prev = cur->next;
                cur->base = 0;
                cur->next = NULL;
            }
            return base;
        }
        prev = &cur->next;
        cur  = cur->next;
    }
    panic("vmm: kernel VA space exhausted");
}

void kvma_free(uintptr_t base, size_t count) {
    
    vma_region_t **prev = &vma_free_list;
    vma_region_t  *cur  = vma_free_list;

    while (cur && cur->base < base) {
        prev = &cur->next;
        cur  = cur->next;
    }

    
    vma_region_t *p = (prev == &vma_free_list) ? NULL
                : container_of(prev, vma_region_t, next);

    if (p && p->base + p->pages * PMM_PAGE_SIZE == base) {
        p->pages += count;
        
        if (cur && p->base + p->pages * PMM_PAGE_SIZE == cur->base) {
            p->pages += cur->pages;
            p->next   = cur->next;
            cur->pages = 0;
        }
        return;
    }

    if (cur && base + count * PMM_PAGE_SIZE == cur->base) {
        cur->base   = base;
        cur->pages += count;
        return;
    }

    vma_region_t *n = kvma_node_alloc();
    n->base  = base;
    n->pages = count;
    n->next  = cur;
    *prev    = n;
}


