#include "Logger.h"

#include "asm/ams.h"
#include "printf/printf.h"
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include "string/string.h"


static uint8_t current_fg = ANSI_WHITE;
static uint8_t current_bg = ANSI_BG_BLACK;


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


#define LOG_BUFFER_SIZE 512
void sys_serial_vlogf(const char* format, const char* file,
    const char* func, int line, va_list args)
{
    char msg[LOG_BUFFER_SIZE];
    vsnprintf(msg, sizeof(msg), format, args);

    if (func != NULL && line != 0) {
        char serial_out[LOG_BUFFER_SIZE];
        snprintf(serial_out, sizeof(serial_out),
                 "< %s:%d(%s)> %s",
                 file, line, func, msg);
        serial_write_string(serial_out);
    } else if (file != NULL && file[0] != '\0') {
        char serial_out[LOG_BUFFER_SIZE];
        snprintf(serial_out, sizeof(serial_out),
                 "< %s> %s",
                 file, msg);
        serial_write_string(serial_out);
    } else {
        serial_write_string(msg);
    }

    printf("%s", msg);
}

	
void sys_serial_logf(const char* format, const char* file, const char* func, int line, ...) {
	va_list args;
    va_start(args, line);
    sys_serial_vlogf(format, file, func, line, args);
    va_end(args);
}


void serial_log_hex(const char* label, uint32_t val) {
	sys_serial_logf("%s: 0x%x\n", NULL, NULL, 0, label, val );
}
