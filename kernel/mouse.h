#pragma once
#include <stdint.h>

struct MouseState {
    int     x, y;
    int     dx, dy;
    bool    left;
    bool    right;
    bool    middle;
    bool    left_click;
    bool    right_click;
    bool    middle_click;
    int8_t  scroll;
};

bool mouse_init(int screen_w, int screen_h);
void mouse_handler();
void mouse_read(MouseState& out);

int  mouse_x();
int  mouse_y();
bool mouse_left();
bool mouse_right();

void mouse_set_callback(void (*cb)(const MouseState&));
void mouse_set_bounds(int w, int h);