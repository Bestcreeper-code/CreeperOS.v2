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

