#include "rtl8139.h"
#include "paging.h"
#include "heap.h"
#include "kstring.h"
#include "terminal.h"

static inline void outb(uint16_t port, uint8_t  v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(port)); }
static inline void outw(uint16_t port, uint16_t v){ __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(port)); }
static inline void outl(uint16_t port, uint32_t v){ __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(port)); }
static inline uint8_t  inb(uint16_t port){ uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint16_t inw(uint16_t port){ uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint32_t inl(uint16_t port){ uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(port)); return v; }

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = (1u<<31) | ((uint32_t)bus<<16) | ((uint32_t)dev<<11)
                  | ((uint32_t)func<<8) | (off & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}
static void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t val) {
    uint32_t addr = (1u<<31) | ((uint32_t)bus<<16) | ((uint32_t)dev<<11)
                  | ((uint32_t)func<<8) | (off & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, val);
}

#define RTL_VENDOR  0x10EC
#define RTL_DEVICE  0x8139

struct PciDevice { uint8_t bus, dev, func; bool found; };

static PciDevice pci_find_rtl8139() {
    PciDevice r = {0,0,0,false};
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read(bus, dev, 0, 0);
            if ((id & 0xFFFF) == RTL_VENDOR && (id >> 16) == RTL_DEVICE) {
                r.bus = bus; r.dev = dev; r.func = 0; r.found = true;
                return r;
            }
        }
    }
    return r;
}

static bool            s_present   = false;
static uint16_t        s_iobase    = 0;
static uint8_t         s_mac[6]    = {};
static uint8_t*        s_rx_buf    = nullptr;
static uint32_t        s_rx_ptr    = 0;
static uint8_t*        s_tx_buf[RTL_TX_NUM];
static uint8_t         s_tx_slot   = 0;
static rtl_rx_callback s_rx_cb     = nullptr;

bool rtl8139_init() {
    s_present = false;

    PciDevice pci = pci_find_rtl8139();
    if (!pci.found) {
        terminal_puts("[RTL8139] Not found on PCI bus\n");
        return false;
    }

    uint32_t cmd = pci_read(pci.bus, pci.dev, pci.func, 0x04);
    cmd |= 0x05;
    pci_write(pci.bus, pci.dev, pci.func, 0x04, cmd);

    uint32_t bar0 = pci_read(pci.bus, pci.dev, pci.func, 0x10);
    s_iobase = (uint16_t)(bar0 & ~3u);

    outb(s_iobase + 0x52, 0x00);

    outb(s_iobase + RTL_REG_CHIPCMD, RTL_CMD_RESET);
    for (int i = 0; i < 100000; i++) {
        if (!(inb(s_iobase + RTL_REG_CHIPCMD) & RTL_CMD_RESET)) break;
    }

    for (int i = 0; i < 6; i++)
        s_mac[i] = inb(s_iobase + RTL_REG_MAC0 + i);

    s_rx_buf = (uint8_t*)kmalloc(RTL_RX_BUF_SIZE);
    if (!s_rx_buf) return false;
    kmemset(s_rx_buf, 0, RTL_RX_BUF_SIZE);

    for (int i = 0; i < RTL_TX_NUM; i++) {
        s_tx_buf[i] = (uint8_t*)kmalloc(RTL_TX_BUF_SIZE);
        if (!s_tx_buf[i]) return false;
    }

    outl(s_iobase + RTL_REG_RXBUF, (uint32_t)s_rx_buf);

    outw(s_iobase + RTL_REG_INTRMASK,
         RTL_INT_ROK | RTL_INT_RER | RTL_INT_TOK | RTL_INT_TER);

    outl(s_iobase + RTL_REG_RXCONFIG,
         RTL_RX_ACCEPT_PHYS | RTL_RX_ACCEPT_BCAST |
         RTL_RX_ACCEPT_MULTI | RTL_RX_WRAP | RTL_RX_BUFSZ_32K);

    outl(s_iobase + RTL_REG_TXCONFIG, 0x03000700);

    outb(s_iobase + RTL_REG_CHIPCMD, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);

    outw(s_iobase + RTL_REG_RXBUFTAIL, 0);
    s_rx_ptr  = 0;
    s_tx_slot = 0;
    s_present = true;

    terminal_puts("[RTL8139] Init OK, MAC: ");
    char tmp[3];
    for (int i = 0; i < 6; i++) {
        uint8_t b = s_mac[i];
        tmp[0] = "0123456789ABCDEF"[b >> 4];
        tmp[1] = "0123456789ABCDEF"[b & 0xF];
        tmp[2] = 0;
        terminal_puts(tmp);
        if (i < 5) terminal_putchar(':');
    }
    terminal_putchar('\n');
    return true;
}

bool rtl8139_present() { return s_present; }

void rtl8139_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = s_mac[i];
}

void rtl8139_set_rx_callback(rtl_rx_callback cb) { s_rx_cb = cb; }

bool rtl8139_send(const uint8_t* data, uint16_t len) {
    if (!s_present || len > RTL_TX_BUF_SIZE) return false;

    uint8_t slot = s_tx_slot;
    for (int i = 0; i < 100000; i++) {
        uint32_t status = inl(s_iobase + RTL_REG_TXSTATUS0 + slot * 4);
        if (!(status & RTL_TX_OWN)) break;
    }

    kmemcpy(s_tx_buf[slot], data, len);
    if (len < 60) {
        kmemset(s_tx_buf[slot] + len, 0, 60 - len);
        len = 60;
    }

    outl(s_iobase + RTL_REG_TXADDR0   + slot * 4, (uint32_t)s_tx_buf[slot]);
    outl(s_iobase + RTL_REG_TXSTATUS0 + slot * 4, len & 0x1FFF);

    s_tx_slot = (s_tx_slot + 1) % RTL_TX_NUM;
    return true;
}

void rtl8139_irq_handler() {
    if (!s_present) return;

    uint16_t status = inw(s_iobase + RTL_REG_INTRSTATUS);
    outw(s_iobase + RTL_REG_INTRSTATUS, status);

    if (status & RTL_INT_ROK) {
        while (!(inb(s_iobase + RTL_REG_CHIPCMD) & 0x01)) {
            uint8_t* pkt = s_rx_buf + s_rx_ptr;

            uint16_t rx_status = (uint16_t)(pkt[0] | (pkt[1] << 8));
            uint16_t rx_len    = (uint16_t)(pkt[2] | (pkt[3] << 8));

            if (rx_len == 0 || rx_len > 1514 + 4) {
                s_rx_ptr = 0;
                outw(s_iobase + RTL_REG_RXBUFTAIL, 0);
                break;
            }

            if ((rx_status & 0x0001) && s_rx_cb) {
                s_rx_cb(pkt + 4, rx_len - 4);
            }

            s_rx_ptr = (s_rx_ptr + rx_len + 4 + 3) & ~3u;
            s_rx_ptr %= (32 * 1024);

            outw(s_iobase + RTL_REG_RXBUFTAIL, (uint16_t)(s_rx_ptr - 16));
        }
    }
}