#include "mouse.h"

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

#define PS2_STATUS_OUTBUF  (1 << 0)
#define PS2_STATUS_INBUF   (1 << 1)
#define PS2_STATUS_MOUSE   (1 << 5)

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile("inb %1,%0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1" :: "a"(val), "Nd"(port));
}
static inline void io_wait() {
    __asm__ volatile("outb %0,$0x80" :: "a"((uint8_t)0));
}

static void ps2_wait_write() {
    int t = 100000;
    while (--t && (inb(PS2_STATUS) & PS2_STATUS_INBUF));
}
static void ps2_wait_read() {
    int t = 100000;
    while (--t && !(inb(PS2_STATUS) & PS2_STATUS_OUTBUF));
}

static void ps2_cmd(uint8_t cmd) {
    ps2_wait_write();
    outb(PS2_CMD, cmd);
}

static void mouse_write(uint8_t byte) {
    ps2_cmd(0xD4);
    ps2_wait_write();
    outb(PS2_DATA, byte);
}

static uint8_t mouse_read_byte() {
    ps2_wait_read();
    return inb(PS2_DATA);
}

static MouseState s_state;
static int        s_screen_w = 1024;
static int        s_screen_h = 768;
static void     (*s_callback)(const MouseState&) = nullptr;

static uint8_t  s_packet[4];
static int      s_packet_idx  = 0;
static int      s_packet_size = 3;

static bool     s_prev_left   = false;
static bool     s_prev_right  = false;
static bool     s_prev_middle = false;

bool mouse_init(int screen_w, int screen_h) {
    s_screen_w = screen_w;
    s_screen_h = screen_h;
    s_state    = {};
    s_state.x  = screen_w / 2;
    s_state.y  = screen_h / 2;

    ps2_cmd(0xA8);
    io_wait();

    ps2_cmd(0x20);
    uint8_t cfg = mouse_read_byte();
    cfg |=  (1 << 1);
    cfg &= ~(1 << 5);
    ps2_cmd(0x60);
    ps2_wait_write();
    outb(PS2_DATA, cfg);

    mouse_write(0xFF);
    uint8_t ack = mouse_read_byte();
    if (ack != 0xFA) return false;
    mouse_read_byte();
    mouse_read_byte();

    auto set_sample = [](uint8_t rate) {
        mouse_write(0xF3); mouse_read_byte();
        mouse_write(rate); mouse_read_byte();
    };
    set_sample(200);
    set_sample(100);
    set_sample(80);

    mouse_write(0xF2);
    mouse_read_byte();
    uint8_t dev_id = mouse_read_byte();
    if (dev_id == 3) {
        s_packet_size = 4;
    }

    set_sample(100);

    mouse_write(0xE8); mouse_read_byte();
    mouse_write(0x03); mouse_read_byte();

    mouse_write(0xF4);
    mouse_read_byte();

    s_packet_idx = 0;
    return true;
}

void mouse_handler() {
    uint8_t byte = inb(PS2_DATA);

    if (s_packet_idx == 0 && !(byte & 0x08)) {
        return;
    }

    s_packet[s_packet_idx++] = byte;

    if (s_packet_idx < s_packet_size) return;

    s_packet_idx = 0;

    uint8_t flags = s_packet[0];

    if ((flags & 0x40) || (flags & 0x80)) return;

    bool left   = (flags & 0x01) != 0;
    bool right  = (flags & 0x02) != 0;
    bool middle = (flags & 0x04) != 0;

    int dx = (int)s_packet[1] - ((flags & 0x10) ? 256 : 0);
    int dy = (int)s_packet[2] - ((flags & 0x20) ? 256 : 0);
    dy = -dy;

    int8_t scroll = 0;
    if (s_packet_size == 4) {
        int8_t raw = (int8_t)s_packet[3];
        scroll = (int8_t)((raw & 0x0F) | ((raw & 0x08) ? 0xF0 : 0));
    }

    s_state.x += dx;
    s_state.y += dy;
    if (s_state.x < 0)            s_state.x = 0;
    if (s_state.y < 0)            s_state.y = 0;
    if (s_state.x >= s_screen_w)  s_state.x = s_screen_w - 1;
    if (s_state.y >= s_screen_h)  s_state.y = s_screen_h - 1;

    s_state.dx = dx;
    s_state.dy = dy;

    s_state.left_click   = left   && !s_prev_left;
    s_state.right_click  = right  && !s_prev_right;
    s_state.middle_click = middle && !s_prev_middle;

    s_state.left   = left;
    s_state.right  = right;
    s_state.middle = middle;
    s_state.scroll = scroll;

    s_prev_left   = left;
    s_prev_right  = right;
    s_prev_middle = middle;

    if (s_callback) s_callback(s_state);
}

void mouse_read(MouseState& out) {
    out = s_state;
    s_state.left_click   = false;
    s_state.right_click  = false;
    s_state.middle_click = false;
    s_state.scroll       = 0;
    s_state.dx           = 0;
    s_state.dy           = 0;
}

int  mouse_x()     { return s_state.x; }
int  mouse_y()     { return s_state.y; }
bool mouse_left()  { return s_state.left; }
bool mouse_right() { return s_state.right; }

void mouse_set_callback(void (*cb)(const MouseState&)) { s_callback = cb; }
void mouse_set_bounds(int w, int h) { s_screen_w = w; s_screen_h = h; }