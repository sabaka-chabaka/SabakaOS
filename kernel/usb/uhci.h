#pragma once
#include <stdint.h>

#define UHCI_TD_COUNT   32
#define UHCI_QH_COUNT    8

#define UHCI_POLL_INTERVAL  8

bool uhci_init();
void uhci_irq_handler();

typedef void (*UhciPollCb)(const uint8_t* data, uint8_t len);

uint8_t uhci_register_interrupt(
    uint8_t addr,
    uint8_t endpoint,
    uint8_t max_packet,
    uint8_t interval,
    UhciPollCb cb
);

bool uhci_control(
    uint8_t  addr,
    uint8_t  bmRequestType,
    uint8_t  bRequest,
    uint16_t wValue,
    uint16_t wIndex,
    uint16_t wLength,
    void*    data
);

bool uhci_set_address(uint8_t new_addr);

bool uhci_get_descriptor(uint8_t addr, uint8_t type, uint8_t idx, void* data, uint16_t len);

bool uhci_set_configuration(uint8_t addr, uint8_t config_val);

bool uhci_hid_set_boot_protocol(uint8_t addr, uint8_t iface);

bool uhci_hid_set_idle(uint8_t addr, uint8_t iface);