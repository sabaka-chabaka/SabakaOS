#include "pci.h"

static inline void outl(uint16_t port, uint32_t v) {
    __asm__ volatile("outl %0,%1" :: "a"(v), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile("inl %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = (1u << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func <<  8)
                  | (off & 0xFC);
    outl(PCI_ADDR, addr);
    return inl(PCI_DATA);
}

void pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t val) {
    uint32_t addr = (1u << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func <<  8)
                  | (off & 0xFC);
    outl(PCI_ADDR, addr);
    outl(PCI_DATA, val);
}

void pci_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint16_t val) {
    uint32_t dword = pci_read32(bus, dev, func, off & ~3u);
    uint32_t mask = 0xFFFFu << ((off & 2) * 8);
    dword = (dword & ~mask) | ((uint32_t)val << ((off & 2) * 8));
    pci_write32(bus, dev, func, off & ~3u, dword);
}

uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t dword = pci_read32(bus, dev, func, off & ~3u);
    return (uint16_t)(dword >> ((off & 2) * 8));
}

uint8_t pci_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t dword = pci_read32(bus, dev, func, off & ~3u);
    return (uint8_t)(dword >> ((off & 3) * 8));
}

static PciDevice fill(uint8_t bus, uint8_t dev, uint8_t func) {
    PciDevice d;
    d.bus    = bus;  d.dev  = dev;  d.func = func;
    d.found  = true;
    uint32_t id  = pci_read32(bus, dev, func, 0x00);
    d.vendor = (uint16_t)(id & 0xFFFF);
    d.device = (uint16_t)(id >> 16);
    uint32_t cls = pci_read32(bus, dev, func, 0x08);
    d.prog_if    = (uint8_t)(cls >>  8);
    d.subclass   = (uint8_t)(cls >> 16);
    d.class_code = (uint8_t)(cls >> 24);
    return d;
}

static const PciDevice NOT_FOUND = {0,0,0,0,0,0,0,0,false};

PciDevice pci_find(uint16_t vendor, uint16_t device_id) {
    for (int bus = 0; bus < 8; bus++)
        for (int dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0x00);
            if ((id & 0xFFFF) != vendor) continue;
            if ((id >> 16)    != device_id) continue;
            return fill((uint8_t)bus, (uint8_t)dev, 0);
        }
    return NOT_FOUND;
}

PciDevice pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if) {
    for (int bus = 0; bus < 8; bus++)
        for (int dev = 0; dev < 32; dev++)
            for (int func = 0; func < 8; func++) {
                uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x00);
                if ((id & 0xFFFF) == 0xFFFF) continue;

                uint32_t cls = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x08);
                uint8_t cc  = (uint8_t)(cls >> 24);
                uint8_t sc  = (uint8_t)(cls >> 16);
                uint8_t pi  = (uint8_t)(cls >>  8);

                if (cc != class_code) continue;
                if (sc != subclass)   continue;
                if (prog_if != 0xFF && pi != prog_if) continue;

                return fill((uint8_t)bus, (uint8_t)dev, (uint8_t)func);
            }
    return NOT_FOUND;
}

void pci_enable(const PciDevice& d) {
    uint32_t cmd = pci_read32(d.bus, d.dev, d.func, 0x04);
    cmd |= (1u << 0)   // I/O space
         | (1u << 1)   // Memory space
         | (1u << 2);  // Bus master
    pci_write32(d.bus, d.dev, d.func, 0x04, cmd);
}

uint32_t pci_bar_addr(const PciDevice &d, uint8_t bar_idx) {
    int32_t bar = pci_read32(d.bus, d.dev, d.func, 0x10 + bar_idx * 4);
    if (bar & 1) return bar & ~0x3u;
    return bar & ~0xFu;
}

bool pci_bar_is_io(const PciDevice &d, uint8_t bar_idx) {
    uint32_t bar = pci_read32(d.bus, d.dev, d.func, 0x10 + bar_idx * 4);
    return (bar & 1) != 0;
}

uint8_t pci_irq_line(const PciDevice& d) {
    return pci_read8(d.bus, d.dev, d.func, 0x3C);
}