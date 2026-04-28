#include "mouse.h"
#include "usb/hid.h"

#define HID_COORD_MAX 0x7FFF

static MouseState  s_state;
static int         s_screen_w = 1024;
static int         s_screen_h = 768;
static void      (*s_callback)(const MouseState&) = nullptr;

static bool s_prev_l = false, s_prev_r = false, s_prev_m = false;

static void on_tablet(const HidTabletState& t) {
    int nx = (int)((uint32_t)t.x * (uint32_t)s_screen_w / (HID_COORD_MAX + 1));
    int ny = (int)((uint32_t)t.y * (uint32_t)s_screen_h / (HID_COORD_MAX + 1));

    if (nx < 0)             nx = 0;
    if (ny < 0)             ny = 0;
    if (nx >= s_screen_w)   nx = s_screen_w - 1;
    if (ny >= s_screen_h)   ny = s_screen_h - 1;

    bool l = (t.buttons & 0x01) != 0;
    bool r = (t.buttons & 0x02) != 0;
    bool m = (t.buttons & 0x04) != 0;

    s_state.dx = nx - s_state.x;
    s_state.dy = ny - s_state.y;
    s_state.x  = nx;
    s_state.y  = ny;

    s_state.left   = l;
    s_state.right  = r;
    s_state.middle = m;
    s_state.left_click   = l && !s_prev_l;
    s_state.right_click  = r && !s_prev_r;
    s_state.middle_click = m && !s_prev_m;
    s_state.scroll = t.wheel;

    s_prev_l = l;
    s_prev_r = r;
    s_prev_m = m;

    if (s_callback) s_callback(s_state);
}

bool mouse_init(int sw, int sh) {
    s_screen_w = sw;
    s_screen_h = sh;
    s_state    = {};
    s_state.x  = sw / 2;
    s_state.y  = sh / 2;

    hid_set_tablet_cb(on_tablet);
    return true;
}

void mouse_handler() {}

void mouse_read(MouseState& out) {
    out = s_state;
    s_state.left_click = s_state.right_click = s_state.middle_click = false;
    s_state.scroll = s_state.dx = s_state.dy = 0;
}

int  mouse_x()     { return s_state.x; }
int  mouse_y()     { return s_state.y; }
bool mouse_left()  { return s_state.left; }
bool mouse_right() { return s_state.right; }

void mouse_set_callback(void (*cb)(const MouseState&)) { s_callback = cb; }
void mouse_set_bounds(int w, int h) { s_screen_w = w; s_screen_h = h; }