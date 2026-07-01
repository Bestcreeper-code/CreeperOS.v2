#pragma once 
#include <stddef.h>
#include <stdint.h>









#if defined (__i386__)

typedef uint32_t register_t;

#elif defined (__x86_64__)

typedef uint64_t register_t;


#endif









#if defined(__x86_64__)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void invlpg(uintptr_t va) {
    __asm__ volatile ("invlpg (%0)" :: "r"(va) : "memory");
}

static inline void cr3_set(uintptr_t pml4_phys) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

static inline uintptr_t cr3_get() {
    uintptr_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
static inline uintptr_t sp_get() {
    uintptr_t sp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(sp));
    return sp;
}

static inline void cli(){
    asm volatile("cli");
} 

static inline void sti(){
    asm volatile("sti");
} 

static inline void hlt(){
    asm volatile("hlt");
} 

typedef struct {
    size_t rax, rbx, rcx, rdx;
    size_t rsi, rdi, rbp, rsp;
    size_t r8,  r9,  r10, r11;
    size_t r12, r13, r14, r15;

    size_t rip, rflags;

    uint16_t cs, ds, es, fs, gs, ss;

    size_t cr0, cr2, cr3, cr4, cr8;
} __attribute__((packed)) cpu_registers_t;


typedef struct {
    uintptr_t top,bottom;
    size_t size;
} Stack_t;


inline void capture_cpu_registers(cpu_registers_t* regs) {
    asm volatile(
        "mov %%rax, 0(%0)\n\t"
        "mov %%rbx, 8(%0)\n\t"
        "mov %%rcx, 16(%0)\n\t"
        "mov %%rdx, 24(%0)\n\t"
        "mov %%rsi, 32(%0)\n\t"
        "mov %%rdi, 40(%0)\n\t"
        "mov %%rbp, 48(%0)\n\t"
        "mov %%rsp, 56(%0)\n\t"

        "mov %%r8,  64(%0)\n\t"
        "mov %%r9,  72(%0)\n\t"
        "mov %%r10, 80(%0)\n\t"
        "mov %%r11, 88(%0)\n\t"
        "mov %%r12, 96(%0)\n\t"
        "mov %%r13, 104(%0)\n\t"
        "mov %%r14, 112(%0)\n\t"
        "mov %%r15, 120(%0)\n\t"

        "pushfq\n\t"
        "popq 128(%0)\n\t"

        "mov %%cs, 136(%0)\n\t"
        "mov %%ds, 138(%0)\n\t"
        "mov %%es, 140(%0)\n\t"
        "mov %%fs, 142(%0)\n\t"
        "mov %%gs, 144(%0)\n\t"
        "mov %%ss, 146(%0)\n\t"

        "lea (%%rip), %%rax\n\t"
        "mov %%rax, 152(%0)\n\t"

        "mov %%cr0, %%rax\n\t"
        "mov %%rax, 160(%0)\n\t"

        "mov %%cr2, %%rax\n\t"
        "mov %%rax, 168(%0)\n\t"

        "mov %%cr3, %%rax\n\t"
        "mov %%rax, 176(%0)\n\t"

        "mov %%cr4, %%rax\n\t"
        "mov %%rax, 184(%0)\n\t"

        "mov %%cr8, %%rax \n\t"
        "mov %%rax, 192(%0)\n\t"

        :
        : "r"(regs)
        : "rax", "memory"
    );
}

inline static uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low; 
}

inline static void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF); 
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}


#else
#error unsupported arch
#endif











#if defined(__i386__) || defined(__x86_64__)
static inline uintptr_t get_instruction_pointer() {
    uintptr_t ip;
    __asm__ volatile (
        "call 1f\n\t"
        "1: pop %0"
        : "=r"(ip)
        :
        : "memory"
    );
    return ip;
}

#else
    #error missing arch implementation
#endif

