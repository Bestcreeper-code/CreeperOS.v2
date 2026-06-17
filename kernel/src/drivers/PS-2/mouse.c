#include "mouse.h"
#include "asm/ams.h"
#include "requests.h"

#include <stdint.h>

#define MAX_MOUSE_X (int)framebuffer_request.response->framebuffers[0]->width
#define MAX_MOUSE_Y (int)framebuffer_request.response->framebuffers[0]->height

#define CURSOR_SIZE  3
#define CURSOR_COLOR 0xFF888888

volatile uint8_t mouse_cycle = 0;
volatile uint8_t mouse_bytes[3];
volatile short mouse_x;
volatile short mouse_y;
volatile uint8_t mouse_buttons;

void mouse_wait(uint8_t type) {
    if (type == 0) {
        while ((inb(0x64) & 1) == 0) {
            __asm__ volatile ("hlt");
        }
    } else {
        while ((inb(0x64) & 2) != 0) {
            __asm__ volatile ("hlt");
        }
    }
}

void mouse_write(uint8_t a_write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, a_write);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void init_mouse() {
    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 4);
    outb(0xA1, mask);

    mouse_wait(1);
    outb(0x64, 0xA8);

    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    uint8_t status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    mouse_x = 160;
    mouse_y = 100;
    mouse_buttons = 0;
}

void mouse_irq_handler() {
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) return;

    uint8_t data = inb(0x60);
    mouse_bytes[mouse_cycle] = data;
    mouse_cycle = (mouse_cycle + 1) % 3;

    if (mouse_cycle == 0 && (mouse_buttons & MOUSE_DISPLAYED_FLAG)) {
        uint8_t buttons = mouse_bytes[0] & 0x07;
        int8_t x_move = (int8_t)mouse_bytes[1];
        int8_t y_move = (int8_t)mouse_bytes[2];

        mouse_buttons = (mouse_buttons & ~0x07) | (buttons & 0x07);

        mouse_x += x_move;
        mouse_y -= y_move;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > MAX_MOUSE_X - 1) mouse_x = MAX_MOUSE_X - 1;
        if (mouse_y > MAX_MOUSE_Y - 1) mouse_y = MAX_MOUSE_Y - 1;
    }
}

bool Get_Mouse_Button(Mouse_FLAGS button) {
    return (mouse_buttons & button) != 0;
}

void Get_Mouse_Pos(short* x, short* y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

static inline void draw_pixel_raw(int x, int y, uint32_t color) {
    if (x < 0 || x >= MAX_MOUSE_X || y < 0 || y >= MAX_MOUSE_Y) return;

    uint32_t* fb = (uint32_t*)framebuffer_request.response->framebuffers[0]->address;
    uint64_t pitch_px = framebuffer_request.response->framebuffers[0]->pitch / 4;

    fb[y * pitch_px + x] = color;
}

void Redraw_Mouse_Cursor() {
    for (int dy = 0; dy < CURSOR_SIZE; dy++) {
        for (int dx = 0; dx < CURSOR_SIZE; dx++) {
            draw_pixel_raw(mouse_x + dx, mouse_y + dy, CURSOR_COLOR);
        }
    }
}

void enable_mouse_display() {
    mouse_buttons |= MOUSE_DISPLAYED_FLAG;
}

void disable_mouse_display() {
    mouse_buttons &= ~MOUSE_DISPLAYED_FLAG;
}