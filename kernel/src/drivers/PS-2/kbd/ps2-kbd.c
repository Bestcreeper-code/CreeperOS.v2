#include "debug/Logger.h"
#include "arch/interrupts.h"
#include "arch/x86_64/cpu/idt.h"
#include "asm/asm.h"
#include "defines/compiler_defs.h"
#include "ps2-kbd.h"
#include "input/input.h"
#include "interrupts/interrupts.h"
#include "memops.h"
#include "drivers/drivers.h"
#include "memory/memory.h"
#include "memory/pmm.h"
#include "scheduler/scheduler.h"
#include "string/format.h"
#include "vfs/vfs.h"
#include "vfs/fs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



uint8_t kbd_mod_keys;
char current_language = KB_LAY_AZERTY;

#define SET_KEYBOARD_MOD_FLAG(flag, state) \
    (kbd_mod_keys) = (state) ? (kbd_mod_keys) | (flag) : ((kbd_mod_keys) & ~(flag))

#define GET_KEYBOARD_MOD_FLAG(flag) \
    ((kbd_mod_keys) & (flag))

#define HARDCODED_PS2_KBD_INTERRUPT_VECTOR_INDEX 33


#define NUM_SCANCODES 128

#define INPUT_CHAR_BUFFER_SIZE 256

volatile uint8_t input_char_buffer[INPUT_CHAR_BUFFER_SIZE];
static volatile uint16_t buf_head = 0;
static volatile uint16_t buf_tail = 0;


const unsigned char keymaps[KB_LAY_COUNT][MOD_COUNT][NUM_SCANCODES] = {
    [KB_LAY_AZERTY] = {
        [MOD_Normal] = {
            0, 27, '&', 'e', '"', '\'', '(', '-', 'e', '_', 'c', 'a', ')', '=', '\b', '\t',
            'a', 'z', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '^', '$', '\n', 0, 'q', 's',
            'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 'u', '`', 0, '*', 'w', 'x', 'c', 'v',
            'b', 'n', ',', ';', ':', '!', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
            '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        [MOD_Shift] = {
            0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '0', '+', '\b', '\t',
            'A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, 0, '\n', 0, 'Q', 'S',
            'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M', '%', '~', 0, '|', 'W', 'X', 'C', 'V',
            'B', 'N', '?', '.', '/', 0, 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
            '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        [MOD_AltGr] = {
            0, 0, 0, '~', '#', '{', '[', '|', '`', '\\', '^', '@', ']', '}', 0, 0,
            0, 0, 'E', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        }
    },
    
    
    [KB_LAY_QWERTY] = {
        [MOD_Normal] = {
            0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
            'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
            'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
            'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
            '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        [MOD_Shift] = {
            0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
            'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
            'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
            'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
            '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        [MOD_AltGr] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        }
    },
};




bool extended = false;

unsigned char get_input_char_non_blocking(void) {
    uint8_t status;
    __asm__ __volatile__("inb $0x64, %0" : "=a"(status));
    if (!(status & 0x01)) {
        extended = false;
        return 0;
    }

    uint8_t scancode;
    __asm__ __volatile__("inb $0x60, %0" : "=a"(scancode));

    if (scancode == 0xE0) {
        extended = true;
        return 0;
    }

    bool released = (scancode & 0x80) != 0;
    uint8_t keycode = scancode & 0x7F;

    if (extended) {
        if (released) {
            if (keycode == 0x1D)
                SET_KEYBOARD_MOD_FLAG(CTRL_PRESSED, false);
            else if (keycode == 0x38)
                SET_KEYBOARD_MOD_FLAG(ALTGR_PRESSED, false);
            extended = false;
            return 0;
        }

        unsigned char c = 0;
        switch (keycode) {
            case 0x48: c = KEY_UP; break;
            case 0x50: c = KEY_DOWN; break;
            case 0x4B: c = KEY_LEFT; break;
            case 0x4D: c = KEY_RIGHT; break;
            case 0x47: c = KEY_HOME; break;
            case 0x1D: SET_KEYBOARD_MOD_FLAG(CTRL_PRESSED, true); break;
            case 0x38: SET_KEYBOARD_MOD_FLAG(ALTGR_PRESSED, true); break;
        }
        extended = false;
        return c;
    }

    switch (keycode) {
        case 0x2A:
        case 0x36:
            SET_KEYBOARD_MOD_FLAG(SHIFT_PRESSED, !released);
            return 0;
        case 0x3A:
            if (!released) {
                bool caps_state = (GET_KEYBOARD_MOD_FLAG(CAPSLOCK_ON) != 0);
                SET_KEYBOARD_MOD_FLAG(CAPSLOCK_ON, !caps_state);
            }
            return 0;
        case 0x1D:
            SET_KEYBOARD_MOD_FLAG(CTRL_PRESSED, !released);
            return 0;
        case 0x38:
            SET_KEYBOARD_MOD_FLAG(ALT_PRESSED, !released);
            return 0;
    }

    if (released) return 0;

    KeyModifier mod = MOD_Normal;
    if (GET_KEYBOARD_MOD_FLAG(ALTGR_PRESSED)) {
        mod = MOD_AltGr;
    } else if ((GET_KEYBOARD_MOD_FLAG(SHIFT_PRESSED) != 0) ^ (GET_KEYBOARD_MOD_FLAG(CAPSLOCK_ON) != 0)) {
        mod = MOD_Shift;
    }

    unsigned char base_char = keymaps[(int)current_language][mod][keycode];

    if ((GET_KEYBOARD_MOD_FLAG(CTRL_PRESSED) != 0) &&
        ((base_char >= 'A' && base_char <= 'Z') || (base_char >= 'a' && base_char <= 'z')) &&
        !GET_KEYBOARD_MOD_FLAG(ALTGR_PRESSED)) {
        base_char = base_char - 'A' + CTRL_KEY_COMBO;
    }
    
    return base_char;
}



// unsigned char getc(){
//     unsigned char chr = 0;
//     while (chr == 0) {
//         __asm__ __volatile__("sti; hlt");
//         if (buf_head != buf_tail) {
//             chr = input_char_buffer[buf_head];
//             buf_head = (buf_head + 1) % INPUT_CHAR_BUFFER_SIZE;
//         }
//     }
//     return chr;
// }


// unsigned char getc_nb(){
//     if (buf_head == buf_tail)
//         return 0;
//     unsigned char chr = input_char_buffer[buf_head];
//     buf_head = (buf_head + 1) % INPUT_CHAR_BUFFER_SIZE;
//     return chr;
// }

GCC_ATTR((interrupt))
void ps2_keyboard_handler(void* frame) {
    unsigned char ch = get_input_char_non_blocking();
    if (ch != 0) {
        uint16_t next_tail = (buf_tail + 1) % INPUT_CHAR_BUFFER_SIZE;
        if (next_tail != buf_head) {
            input_char_buffer[buf_tail] = ch;
            buf_tail = next_tail;
        }
        // Sys_log_NoPos("%c",ch);
    }

    if(ch=='l') _log_all_processes();
    if(ch=='t') fs_tree(root_dentry, 0);
    if(ch=='m') {
        char buff[100];
        byte_nb_simplify(pmm_get_total_pages() - pmm_get_free_pages(), buff, 4);
        Sys_log_NoPos(ESC_BG_YELLOW"Ram: %s", buff);
        byte_nb_simplify(pmm_get_total_pages(), buff, 4);
        Sys_log_NoPos("/ %s"ESC_RESET"\n", buff);
    }
    _current_eoi((uint8_t)HARDCODED_PS2_KBD_INTERRUPT_VECTOR_INDEX);
}




int ps2_kbd_read(struct input_device *dev, void *buf, size_t count, loff_t offset) {
    

    size_t bytes_read = 0;
    unsigned char *out = (unsigned char *)buf;

    while (bytes_read < count) {
        unsigned char ch = 0;
        while (ch == 0) {
            // __asm__ __volatile__(" hlt");
            if (buf_head != buf_tail) {
                ch = input_char_buffer[buf_head];
                buf_head = (buf_head + 1) % INPUT_CHAR_BUFFER_SIZE;
            }
        }

        out[bytes_read++] = ch;

    }

    return (int)bytes_read;
}

struct input_device_ops ps2_kbd_ops = {
    .read  = ps2_kbd_read,
    .write = vfs_inv_func,
    .ioctl = (int (*)(struct input_device *, int, ...))vfs_inv_func
};

int ps2_kbd_init() {
    idt_set_allocated(HARDCODED_PS2_KBD_INTERRUPT_VECTOR_INDEX);
    setup_interrupt_vector(HARDCODED_PS2_KBD_INTERRUPT_VECTOR_INDEX, ps2_keyboard_handler, IRQ_FLAG_INTERRUPT);

    outb(0x64, 0xAE);
    outb(0x60, 0xF4);

    while (!(inb(0x64) & 0x01));
    uint8_t ack = inb(0x60);
    Sys_Info("ACK: %02x\n", ack);

    struct input_device* kbd_dev = register_input_device(
        "PS/2 Keyboard",
        &ps2_kbd_ops,
        NULL
    );

    if (kbd_dev == NULL) {
        Sys_Error("couldn't register ps2 in input registery\n");
        return -1;
    }

    return 0;
}
REGISTER_DRIVER_DEV(ps_2_kbd, ps2_kbd_init);