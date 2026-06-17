#ifndef MOUSE_H
#define MOUSE_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOUSE_BUTTON_LEFT = 1<<0,
    MOUSE_BUTTON_RIGHT = 1<<1,
    MOUSE_BUTTON_MIDDLE = 1<<2,


    MOUSE_DISPLAYED_FLAG = 1<<7
} Mouse_FLAGS;

typedef enum {
    Curs_state_Base,
    Curs_state_Base_SpinLoad,
    Curs_state_Base_Qmark,
    Curs_state_Empty1,
    Curs_state_X,
    Curs_state_Hglass,
    Curs_state_arr_UL,
    Curs_state_zoom,
    Curs_state_unzoom,
    Curs_state_Empty2,
    Curs_state_Write,
    Curs_state_width_mod,
    Curs_state_height_mod,
    Curs_state_Empty3,
    Curs_state_Hand_Point,
    Curs_state_Hand_Ungrabbed,
    Curs_state_Hand_Grabbed,
} Cursorstate;

void init_mouse();

void mouse_irq_handler(); 

void Redraw_Mouse_Cursor();

void enable_mouse_display();
void disable_mouse_display();

bool Get_Mouse_Button(Mouse_FLAGS button);
void Get_Mouse_Pos(short* x, short* y);

void change_mouse_state(Cursorstate state);

extern uint16_t mouse_x_pos_prev;
extern uint16_t mouse_y_pos_prev;

#endif // MOUSE_H
