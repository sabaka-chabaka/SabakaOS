#pragma once
#include <stdint.h>

struct PciDevice {
    uint8_t bus, dev, func;
    uint16_t vendor, device;
    uint8_t class_code, subclass, prog_if;
    bool found;
};

uint32_t pci_read32 (uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
void     pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t val);

uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
uint8_t  pci_read8 (uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);

PciDevice pci_find(uint16_t vendor, uint16_t device_id);

PciDevice pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if);

void pci_enable(const PciDevice& d);

uint32_t pci_bar_addr(const PciDevice& d, uint8_t bar_idx);
bool     pci_bar_is_io(const PciDevice& d, uint8_t bar_idx);

uint8_t pci_irq_line(const PciDevice& d);