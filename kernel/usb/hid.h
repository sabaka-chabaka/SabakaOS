#pragma once
#include <stdint.h>

bool hid_init();

enum HidDeviceType {
    HID_NONE = 0,
    HID_TABLET = 1,
    HID_KEYBOARD = 2,
    HID_MOUSE = 3
};

struct HidTabletState {
    uint16_t x, y;
    uint8_t  buttons;
    int8_t   wheel;
};

struct HidKeyState {
    uint8_t modifier;
    uint8_t keycodes[6];
};

void hid_set_tablet_cb  (void (*cb)(const HidTabletState&));
void hid_set_keyboard_cb(void (*cb)(const HidKeyState&));