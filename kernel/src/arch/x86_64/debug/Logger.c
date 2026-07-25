#include "debug/Logger.h"

#include "arch/x86_64/scheduler/scheduler.h"
#include "asm/asm.h"
#include "defines/compiler_defs.h"
#include "memory/memory.h"
#include "mm/vmm_arch.h"
#include "printf/printf.h"
#include <stdbool.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include "string/string.h"
#include "memops.h"
#include "timer/time.h"




static uint8_t current_fg = ANSI_WHITE;
static uint8_t current_bg = ANSI_BG_BLACK;

static volatile bool is_threaded = false;

#define LOG_BUFFER_SIZE 512
#define LOG_RING_PAGES 64

typedef struct {
    char buf[LOG_BUFFER_SIZE];
    int ready;
} log_slot_t;

#define LOG_RING_QUEUE_SIZE ((LOG_RING_PAGES * PMM_PAGE_SIZE) / sizeof(log_slot_t))

static log_slot_t *log_ring_queue;
static uint32_t log_ring_queue_write_idx;
static uint32_t log_ring_queue_read_idx;


void serial_init() {
	outb(COM1_PORT + 1, 0x00); // Disable interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x03); // Set divisor to 3 (38400 baud)
    outb(COM1_PORT + 1, 0x00); //                  (high byte)
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7); // Enable FIFO, clear them, 14-byte threshold
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

int serial_is_transmit_ready() {
	return inb(COM1_PORT + 5) & 0x20;
}

void serial_write_char(char c) {
	while (!serial_is_transmit_ready());
    outb(COM1_PORT, c);
}

void serial_write_string(const char* str) {
	while (*str) {
		if (*str == '\n') {
			serial_write_char('\r');
        }
        serial_write_char(*str++);
    }
}

void log_queue_init() {
    uintptr_t va = page_kalloc(LOG_RING_PAGES, PTE_PRESENT | PTE_WRITABLE);
    log_ring_queue = (log_slot_t *)va;
    memset(log_ring_queue, 0, LOG_RING_PAGES * PMM_PAGE_SIZE);
    atomic_store(&log_ring_queue_write_idx, 0);
    log_ring_queue_read_idx = 0;
}

void _log_manager_thread() {
    is_threaded = true;

    while (true) {
        log_slot_t *slot = &log_ring_queue[log_ring_queue_read_idx];

        if (!atomic_load_explicit(&slot->ready, memory_order_acquire)) {
            hlt();
            continue;
        }

        serial_write_string(slot->buf);
        printf("%s", slot->buf);

        atomic_store_explicit(&slot->ready, 0, memory_order_release);
        log_ring_queue_read_idx = (log_ring_queue_read_idx + 1) % LOG_RING_QUEUE_SIZE;
    }
}

void sys_serial_vlogf(const char* format, const char* file,
    const char* func, int line, va_list args)
{
    uint64_t ms = timer_get_ms();
    uint64_t secs = ms / 1000;
    uint64_t millis = ms % 1000;

    if (!is_threaded) {
        char msg[LOG_BUFFER_SIZE];
        vsnprintf(msg, sizeof(msg), format, args);

        if (func && line) {
            char serial_out[LOG_BUFFER_SIZE];
            snprintf(serial_out, sizeof(serial_out),
                "\e[0m[%llu.%03llu] < %s:%d(%s)> %s",
                (unsigned long long)secs, (unsigned long long)millis,
                file, line, func, msg);
            serial_write_string(serial_out);
        } else if (file && file[0]) {
            char serial_out[LOG_BUFFER_SIZE];
            snprintf(serial_out, sizeof(serial_out),
                "\e[0m[%llu.%03llu] < %s> %s",
                (unsigned long long)secs, (unsigned long long)millis,
                file, msg);
            serial_write_string(serial_out);
        } else {
            char serial_out[LOG_BUFFER_SIZE];
            snprintf(serial_out, sizeof(serial_out), "%s", msg);
            serial_write_string(serial_out);
        }
        if(!func && !line){
            printf("%s",
                msg);
        } else {
            printf("[%llu.%03llu] %s",
                (unsigned long long)secs, (unsigned long long)millis, msg);
        }
        return;
    }

    uint32_t idx = atomic_fetch_add_explicit(&log_ring_queue_write_idx, 1, memory_order_relaxed)
        % LOG_RING_QUEUE_SIZE;
    log_slot_t *slot = &log_ring_queue[idx];
    if (atomic_load_explicit(&slot->ready, memory_order_acquire)) {
        return;
    }

    int n;
    if(!file && !func && !line){
        
        n = 0;
    }else {

        n = snprintf(slot->buf, LOG_BUFFER_SIZE, "\e[0m[%llu.%03llu] ",
            (unsigned long long)secs, (unsigned long long)millis);
    }

    if (n > 0 && (size_t)n < LOG_BUFFER_SIZE) {
        vsnprintf(slot->buf + n, LOG_BUFFER_SIZE - n, format, args);
    } else {
        vsnprintf(slot->buf, LOG_BUFFER_SIZE, format, args);
    }
    
    atomic_store_explicit(&slot->ready, 1, memory_order_release);
}

	
void sys_serial_logf(const char* format, const char* file, const char* func, int line, ...) {
	va_list args;
    va_start(args, line);
    sys_serial_vlogf(format, file, func, line, args);
    va_end(args);
}


void serial_log_hex(const char* label, uint32_t val) {
	char buffer[64];
    sprintf(buffer,"%s: 0x%x", label, val);
    serial_write_string(buffer);
}


void ktty_write(const char* data, size_t len) {
    if (!data || !len) return;

    size_t written = 0;

    while (written < len) {
        size_t chunk = len - written;
        if (chunk > LOG_BUFFER_SIZE - 1)
            chunk = LOG_BUFFER_SIZE - 1;

        if (!is_threaded) {
            char msg[LOG_BUFFER_SIZE];
            memcpy(msg, data + written, chunk);
            msg[chunk] = '\0';

            serial_write_string(msg);
            printf("%s", msg);
        } else {
            uint32_t idx = atomic_fetch_add_explicit(&log_ring_queue_write_idx, 1,
                memory_order_relaxed) % LOG_RING_QUEUE_SIZE;
            log_slot_t *slot = &log_ring_queue[idx];

            if (!atomic_load_explicit(&slot->ready, memory_order_acquire)) {
                memcpy(slot->buf, data + written, chunk);
                slot->buf[chunk] = '\0';
                atomic_store_explicit(&slot->ready, 1, memory_order_release);
            }
            
        }

        written += chunk;
    }
}

void dump_userspace_mappings(uint64_t* pml4) {
    Sys_Debug("---- userspace mapping dump (PML4[0..255]) ----\n");
    Sys_Debug("pml4 virt is %p\n",pml4);

    for (int pml4_i = 0; pml4_i < 256; pml4_i++) {
        uint64_t pml4_e = pml4[pml4_i];
        if (!(pml4_e & PTE_PRESENT)) continue;

        uint64_t* pdpt = (uint64_t*)((pml4_e & PTE_ADDR_MASK) + hhdm_offset);

        for (int pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            uint64_t pdpt_e = pdpt[pdpt_i];
            if (!(pdpt_e & PTE_PRESENT)) continue;

            uintptr_t va_pdpt = ((uintptr_t)pml4_i << 39) | ((uintptr_t)pdpt_i << 30);

            if (pdpt_e & PTE_HUGE) {
                Sys_Debug("1G  VA=%p -> PA=%p  flags: %s %s %s %s\n",
                    (void*)va_pdpt,
                    (void*)(pdpt_e & PTE_ADDR_MASK),
                    (pdpt_e & PTE_WRITABLE) ? "W" : "-",
                    (pdpt_e & PTE_USER)     ? "U" : "-",
                    (pdpt_e & PTE_NX)       ? "NX" : "X",
                    (pdpt_e & PTE_LOCAL_OWNED)  ? "O" : "NO");
                continue;
            }

            uint64_t* pd = (uint64_t*)((pdpt_e & PTE_ADDR_MASK) + hhdm_offset);

            for (int pd_i = 0; pd_i < 512; pd_i++) {
                uint64_t pd_e = pd[pd_i];
                if (!(pd_e & PTE_PRESENT)) continue;

                uintptr_t va_pd = va_pdpt | ((uintptr_t)pd_i << 21);

                if (pd_e & PTE_HUGE) {
                    Sys_Debug("2M  VA=%p -> PA=%p  flags: %s %s %s %s\n",
                        (void*)va_pd,
                        (void*)(pd_e & PTE_ADDR_MASK),
                        (pd_e & PTE_WRITABLE) ? "W" : "-",
                        (pd_e & PTE_USER)     ? "U" : "-",
                        (pd_e & PTE_NX)       ? "NX" : "X",
                        (pd_e & PTE_LOCAL_OWNED)  ? "O" : "NO");
                    continue;
                }

                uint64_t* pt = (uint64_t*)((pd_e & PTE_ADDR_MASK) + hhdm_offset);

                for (int pt_i = 0; pt_i < 512; pt_i++) {
                    uint64_t pt_e = pt[pt_i];
                    if (!(pt_e & PTE_PRESENT)) continue;

                    uintptr_t va = va_pd | ((uintptr_t)pt_i << 12);

                    Sys_Debug("4K  VA=%p -> PA=%p  flags: %s %s %s %s\n",
                        (void*)va,
                        (void*)(pt_e & PTE_ADDR_MASK),
                        (pt_e & PTE_WRITABLE) ? "W" : "-",
                        (pt_e & PTE_USER)     ? "U" : "-",
                        (pt_e & PTE_NX)       ? "NX" : "X",
                        (pt_e & PTE_LOCAL_OWNED)  ? "O" : "NO");
                }
            }
        }
    }

    Sys_Debug("---- end mapping dump ----\n");
}


