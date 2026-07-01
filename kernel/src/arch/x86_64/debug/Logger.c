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


static uint8_t current_fg = ANSI_WHITE;
static uint8_t current_bg = ANSI_BG_BLACK;

static volatile bool is_threaded = false;

#define LOG_BUFFER_SIZE 256
#define LOG_RING_PAGES 64

typedef struct {
    char buf[LOG_BUFFER_SIZE];
    _Atomic int ready;
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
    if (!is_threaded) {
        char msg[LOG_BUFFER_SIZE];
        vsnprintf(msg, sizeof(msg), format, args);

        if (func && line) {
            char serial_out[LOG_BUFFER_SIZE];
            snprintf(serial_out, sizeof(serial_out),
                "< %s:%d(%s)> %s",
                file, line, func, msg);
            serial_write_string(serial_out);
        } else if (file && file[0]) {
            char serial_out[LOG_BUFFER_SIZE];
            snprintf(serial_out, sizeof(serial_out),
                "< %s> %s",
                file, msg);
            serial_write_string(serial_out);
        } else {
            serial_write_string(msg);
        }

        printf("%s", msg);
        return;
    }

    uint32_t idx = atomic_fetch_add_explicit(&log_ring_queue_write_idx, 1, memory_order_relaxed)
                   % LOG_RING_QUEUE_SIZE;
    log_slot_t *slot = &log_ring_queue[idx];

    if (atomic_load_explicit(&slot->ready, memory_order_acquire)) {
        return;
    }

    vsnprintf(slot->buf, LOG_BUFFER_SIZE, format, args);

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