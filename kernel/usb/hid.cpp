#include "hid.h"
#include "uhci.h"
#include "../terminal.h"
#include <stddef.h>

struct UsbDeviceDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed));

struct UsbConfigDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed));

struct UsbInterfaceDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed));

struct UsbEndpointDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed));

struct UsbHidDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bReportDescriptorType;
    uint16_t wReportDescriptorLength;
} __attribute__((packed));

static void (*s_tablet_cb)  (const HidTabletState&)  = nullptr;
static void (*s_keyboard_cb)(const HidKeyState&)      = nullptr;

void hid_set_tablet_cb  (void (*cb)(const HidTabletState&))  { s_tablet_cb   = cb; }
void hid_set_keyboard_cb(void (*cb)(const HidKeyState&))     { s_keyboard_cb = cb; }

static void on_tablet_report(const uint8_t* data, uint8_t len) {
    if (len < 6 || !s_tablet_cb) return;

    HidTabletState st;
    st.buttons = data[0] & 0x07;
    st.x       = (uint16_t)(data[1] | ((uint16_t)data[2] << 8));
    st.y       = (uint16_t)(data[3] | ((uint16_t)data[4] << 8));
    st.wheel   = (int8_t)data[5];
    s_tablet_cb(st);
}

static void on_keyboard_report(const uint8_t* data, uint8_t len) {
    if (len < 8 || !s_keyboard_cb) return;

    HidKeyState ks;
    ks.modifier = data[0];
    for (int i = 0; i < 6; i++)
        ks.keycodes[i] = data[2 + i];
    s_keyboard_cb(ks);
}

struct FoundIface {
    uint8_t iface_num;
    uint8_t protocol;
    uint8_t endp_addr;
    uint8_t max_packet;
    uint8_t interval;
};

static int parse_config(const uint8_t* buf, uint16_t total, FoundIface* out, int max_ifaces) {
    int found = 0;
    const uint8_t* p = buf;
    const uint8_t* end = buf + total;
    FoundIface cur = {};
    bool in_hid_iface = false;

    while (p < end && found < max_ifaces) {
        uint8_t len  = p[0];
        uint8_t type = p[1];
        if (len < 2) break;

        if (type == 0x04) {
            const UsbInterfaceDescriptor* ifd = (const UsbInterfaceDescriptor*)p;
            if (ifd->bInterfaceClass == 0x03 && ifd->bInterfaceSubClass == 0x01) {
                in_hid_iface = true;
                cur = {};
                cur.iface_num = ifd->bInterfaceNumber;
                cur.protocol  = ifd->bInterfaceProtocol;
            } else {
                in_hid_iface = false;
            }
        } else if (type == 0x05 && in_hid_iface) {
            const UsbEndpointDescriptor* epd = (const UsbEndpointDescriptor*)p;
            if ((epd->bmAttributes & 0x03) == 0x03 && (epd->bEndpointAddress & 0x80)) {
                cur.endp_addr  = epd->bEndpointAddress & 0x0F;
                cur.max_packet = (uint8_t)(epd->wMaxPacketSize & 0xFF);
                cur.interval   = epd->bInterval;
                out[found++]   = cur;
                in_hid_iface   = false;
            }
        }
        p += len;
    }
    return found;
}

static uint8_t s_next_addr = 1;

static bool enumerate_device() {
    static uint8_t desc_buf[256];

    if (!uhci_get_descriptor(0, 0x01, 0, desc_buf, 8)) {
        terminal_puts("[HID] get device desc failed\n");
        return false;
    }

    uint8_t addr = s_next_addr++;
    if (!uhci_set_address(addr)) {
        terminal_puts("[HID] set address failed\n");
        return false;
    }

    for (volatile int i = 0; i < 100000; i++);

    if (!uhci_get_descriptor(addr, 0x01, 0, desc_buf, 18)) {
        terminal_puts("[HID] get full device desc failed\n");
        return false;
    }

    if (!uhci_get_descriptor(addr, 0x02, 0, desc_buf, 9)) {
        terminal_puts("[HID] get config desc failed\n");
        return false;
    }
    uint16_t total = (uint16_t)(desc_buf[2] | ((uint16_t)desc_buf[3] << 8));
    if (total > sizeof(desc_buf)) total = sizeof(desc_buf);
    if (!uhci_get_descriptor(addr, 0x02, 0, desc_buf, total)) {
        terminal_puts("[HID] get full config failed\n");
        return false;
    }
    uint8_t config_val = desc_buf[5];

    FoundIface ifaces[4];
    int n = parse_config(desc_buf, total, ifaces, 4);
    if (n == 0) {
        terminal_puts("[HID] no HID boot ifaces found\n");
        return false;
    }

    if (!uhci_set_configuration(addr, config_val)) {
        terminal_puts("[HID] set config failed\n");
        return false;
    }

    for (int i = 0; i < n; i++) {
        FoundIface& f = ifaces[i];

        uhci_hid_set_boot_protocol(addr, f.iface_num);
        uhci_hid_set_idle(addr, f.iface_num);

        UhciPollCb cb = nullptr;
        const char* type_str = "unknown";

        if (f.protocol == 2) {
            cb = on_tablet_report;
            type_str = "tablet/mouse";
        } else if (f.protocol == 1) {
            cb = on_keyboard_report;
            type_str = "keyboard";
        }

        if (cb) {
            uint8_t handle = uhci_register_interrupt(
                addr, f.endp_addr, f.max_packet, f.interval, cb);
            if (handle) {
                terminal_puts("[HID] registered ");
                terminal_puts(type_str);
                terminal_puts("\n");
            }
        }
    }
    return true;
}

bool hid_init() {
    terminal_puts("[HID] Enumerating USB devices...\n");
    bool ok = false;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (enumerate_device()) ok = true;
    }
    return ok;
}