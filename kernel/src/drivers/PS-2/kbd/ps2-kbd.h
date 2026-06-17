#pragma once


#define KEY_ESCAPE     27
#define KEY_BACKSPACE  8
#define KEY_ENTER      '\n'
#define KEY_UP         0x80
#define KEY_DOWN       0x81
#define KEY_LEFT       0x82
#define KEY_RIGHT      0x83
#define KEY_HOME       0x84
#define CTRL_KEY_COMBO 159


#define ControlCombo(key) CTRL_KEY_COMBO + key - 'A'

#define ALT_PRESSED 0x1
#define ALTGR_PRESSED 0x2
#define CTRL_PRESSED 0x4  
#define SHIFT_PRESSED 0x08
#define CAPSLOCK_ON 0x10


typedef enum {
    KB_LAY_AZERTY,
    KB_LAY_QWERTY,
    KB_LAY_COUNT
} KeyboardLayout;


typedef enum {
    MOD_Normal = 0,
    MOD_Shift = 1,
    MOD_AltGr = 2,
    MOD_COUNT 
} KeyModifier;


