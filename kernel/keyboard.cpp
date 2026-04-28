#include "keyboard.h"
#include "usb/hid.h"

static const char HID_MAP[256] = {
    0,    0,    0,    0,    // 0x00-0x03: reserved / error
    'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  // 0x04-0x0B
    'i',  'j',  'k',  'l',  'm',  'n',  'o',  'p',  // 0x0C-0x13
    'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  // 0x14-0x1B
    'y',  'z',                                         // 0x1C-0x1D
    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0', // 0x1E-0x27
    '\n', 27,   8,    '\t', ' ',                       // 0x28-0x2C Enter/Esc/BS/Tab/Space
    '-',  '=',  '[',  ']',  '\\', 0,    ';',  '\'',  // 0x2D-0x34
    '`',  ',',  '.',  '/',                             // 0x35-0x38
    0,                                                  // 0x39 Caps Lock
    0,0,0,0,0,0,0,0,0,0,0,0,                          // 0x3A-0x45 F1-F12
    0,0,0,0,0,0,0,0,0,0,0,0,0,                        // 0x46-0x52 Print/Scroll/Pause/Insert/Home/PgUp/Del/End/PgDn/Right/Left/Down/Up
    0,                                                  // 0x53 NumLock
    '/',  '*',  '-',  '+',  '\n',                      // 0x54-0x58 Numpad
    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0', '.', // 0x59-0x63
};

static const char HID_SHIFT_MAP[256] = {
    0,    0,    0,    0,
    'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',
    'I',  'J',  'K',  'L',  'M',  'N',  'O',  'P',
    'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',
    'Y',  'Z',
    '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',
    '\n', 27,   8,    '\t', ' ',
    '_',  '+',  '{',  '}',  '|',  0,    ':',  '"',
    '~',  '<',  '>',  '?',
    0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,
    '/',  '*',  '-',  '+',  '\n',
    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0', '.',
};

#define MOD_LCTRL   (1<<0)
#define MOD_LSHIFT  (1<<1)
#define MOD_LALT    (1<<2)
#define MOD_LGUI    (1<<3)
#define MOD_RCTRL   (1<<4)
#define MOD_RSHIFT  (1<<5)
#define MOD_RALT    (1<<6)
#define MOD_RGUI    (1<<7)
#define MOD_SHIFT   (MOD_LSHIFT | MOD_RSHIFT)
#define MOD_CTRL    (MOD_LCTRL  | MOD_RCTRL)

static const int BUF_SIZE = 256;
static char  buf[BUF_SIZE];
static int   buf_head = 0;
static int   buf_tail = 0;
static bool  caps     = false;
static void (*user_cb)(char) = nullptr;

static uint8_t s_prev_keys[6] = {};

static void buf_push(char c) {
    int next = (buf_head + 1) % BUF_SIZE;
    if (next != buf_tail) {
        buf[buf_head] = c;
        buf_head = next;
    }
}

static void on_keyboard(const HidKeyState& ks) {
    bool shift = (ks.modifier & MOD_SHIFT) != 0;

    for (int i = 0; i < 6; i++) {
        uint8_t kc = ks.keycodes[i];
        if (kc != 0x39) continue;
        bool was_pressed = false;
        for (int j = 0; j < 6; j++)
            if (s_prev_keys[j] == 0x39) { was_pressed = true; break; }
        if (!was_pressed) caps = !caps;
    }

    for (int i = 0; i < 6; i++) {
        uint8_t kc = ks.keycodes[i];
        if (!kc || kc == 0x01) continue;

        bool already = false;
        for (int j = 0; j < 6; j++)
            if (s_prev_keys[j] == kc) { already = true; break; }
        if (already) continue;

        char c = 0;
        if (shift) {
            c = (kc < 256) ? HID_SHIFT_MAP[kc] : 0;
        } else {
            c = (kc < 256) ? HID_MAP[kc] : 0;
        }

        if (caps) {
            if (c >= 'a' && c <= 'z') c -= 32;
            else if (c >= 'A' && c <= 'Z' && !shift) c += 32;
        }

        if (!c) continue;
        buf_push(c);
        if (user_cb) user_cb(c);
    }

    for (int i = 0; i < 6; i++) s_prev_keys[i] = ks.keycodes[i];
}

void keyboard_init() {
    buf_head = buf_tail = 0;
    caps = false;
    user_cb = nullptr;
    for (int i = 0; i < 6; i++) s_prev_keys[i] = 0;
    hid_set_keyboard_cb(on_keyboard);
}

void keyboard_set_callback(void (*cb)(char)) { user_cb = cb; }

bool keyboard_haschar() { return buf_head != buf_tail; }

char keyboard_getchar() {
    if (!keyboard_haschar()) return 0;
    char c = buf[buf_tail];
    buf_tail = (buf_tail + 1) % BUF_SIZE;
    return c;
}

uint32_t keyboard_wait_key() {
    while (!keyboard_haschar())
        __asm__ volatile("pause");
    return (uint32_t)(uint8_t)keyboard_getchar();
}

void keyboard_handler() {}