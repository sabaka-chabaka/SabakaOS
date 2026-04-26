#include "mouse.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(port)); return r;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}
static inline void io_wait() {
    outb(0x80, 0);
}

static void ps2_wait_in() {
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & 0x02)) return;
}
static void ps2_wait_out() {
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 0x01) return;
}
static uint8_t ps2_read() {
    ps2_wait_out(); return inb(PS2_DATA);
}
static void ps2_cmd(uint8_t cmd) {
    ps2_wait_in(); outb(PS2_CMD, cmd);
}
static void mouse_write(uint8_t b) {
    ps2_cmd(0xD4);
    ps2_wait_in(); outb(PS2_DATA, b);
}
static uint8_t mouse_read() {
    return ps2_read();
}

static bool mouse_cmd(uint8_t cmd) {
    mouse_write(cmd);
    return mouse_read() == 0xFA;
}

static MouseState s_state;
static int        s_screen_w  = 1024;
static int        s_screen_h  = 768;
static void     (*s_callback)(const MouseState&) = nullptr;

static uint8_t s_packet[4];
static int     s_idx  = 0;
static int     s_size = 3;

static bool s_prev_l = false, s_prev_r = false, s_prev_m = false;

bool mouse_init(int sw, int sh) {
    s_screen_w = sw;
    s_screen_h = sh;
    s_state    = {};
    s_state.x  = sw / 2;
    s_state.y  = sh / 2;
    s_idx      = 0;
    s_size     = 3;

    for (int i = 0; i < 16; i++) {
        if (!(inb(PS2_STATUS) & 0x01)) break;
        inb(PS2_DATA);
    }

    ps2_cmd(0xA8); io_wait();

    ps2_cmd(0x20);
    uint8_t cfg = ps2_read();
    cfg |=  (1 << 1);
    cfg &= ~(1 << 5);
    ps2_cmd(0x60);
    ps2_wait_in(); outb(PS2_DATA, cfg);

    if (!mouse_cmd(0xF6)) {}

    mouse_cmd(0xF3); mouse_write(200); mouse_read();
    mouse_cmd(0xF3); mouse_write(100); mouse_read();
    mouse_cmd(0xF3); mouse_write(80);  mouse_read();

    mouse_write(0xF2);
    if (mouse_read() == 0xFA) {
        uint8_t id = mouse_read();
        if (id == 3 || id == 4) s_size = 4;
    }

    mouse_cmd(0xF3); mouse_write(100); mouse_read();

    mouse_cmd(0xF4);

    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 4);
    outb(0xA1, mask);

    return true;
}

void mouse_handler() {
    uint8_t byte = inb(PS2_DATA);

    if (s_idx == 0 && !(byte & 0x08)) return;

    s_packet[s_idx++] = byte;
    if (s_idx < s_size) return;
    s_idx = 0;

    uint8_t flags = s_packet[0];

    if ((flags & 0xC0)) return;

    bool l = (flags & 0x01) != 0;
    bool r = (flags & 0x02) != 0;
    bool m = (flags & 0x04) != 0;

    int dx =  (int)s_packet[1] - ((flags & 0x10) ? 256 : 0);
    int dy = -((int)s_packet[2] - ((flags & 0x20) ? 256 : 0));

    int8_t scroll = (s_size == 4) ?
        (int8_t)((s_packet[3] & 0x0F) | ((s_packet[3] & 0x08) ? 0xF0 : 0)) : 0;

    s_state.x += dx; s_state.y += dy;
    if (s_state.x < 0)           s_state.x = 0;
    if (s_state.y < 0)           s_state.y = 0;
    if (s_state.x >= s_screen_w) s_state.x = s_screen_w - 1;
    if (s_state.y >= s_screen_h) s_state.y = s_screen_h - 1;

    s_state.dx = dx; s_state.dy = dy; s_state.scroll = scroll;
    s_state.left   = l; s_state.right  = r; s_state.middle = m;
    s_state.left_click   = l && !s_prev_l;
    s_state.right_click  = r && !s_prev_r;
    s_state.middle_click = m && !s_prev_m;
    s_prev_l = l; s_prev_r = r; s_prev_m = m;

    if (s_callback) s_callback(s_state);
}

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