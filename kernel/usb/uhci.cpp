#include "uhci.h"
#include "../pci.h"
#include "../paging.h"
#include "../heap.h"
#include "../terminal.h"
#include <stddef.h>

static inline void     outb(uint16_t p, uint8_t  v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void     outw(uint16_t p, uint16_t v){ __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline void     outl(uint16_t p, uint32_t v){ __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t  inb (uint16_t p){ uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint16_t inw (uint16_t p){ uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint32_t inl (uint16_t p){ uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void io_delay(){ __asm__ volatile("nop;nop;nop;nop"); }

#define UHCI_USBCMD    0x00   
#define UHCI_USBSTS    0x02   
#define UHCI_USBINTR   0x04   
#define UHCI_FRNUM     0x06   
#define UHCI_FRBASEADD 0x08   
#define UHCI_SOFMOD    0x0C   
#define UHCI_PORTSC0   0x10   
#define UHCI_PORTSC1   0x12   

#define CMD_RS      (1<<0)  
#define CMD_HCRESET (1<<1)  
#define CMD_GRESET  (1<<2)  
#define CMD_EGSM    (1<<3)  
#define CMD_FGR     (1<<4)  
#define CMD_SWDBG   (1<<5)  
#define CMD_CF      (1<<6)  
#define CMD_MAXP    (1<<7)  

#define STS_USBINT  (1<<0)  
#define STS_ERROR   (1<<1)  
#define STS_RD      (1<<2)  
#define STS_HSE     (1<<3)  
#define STS_HCPE    (1<<4)  
#define STS_HCH     (1<<5)  

#define PORT_CCS    (1<<0)  
#define PORT_CSC    (1<<1)  
#define PORT_PED    (1<<2)  
#define PORT_PEDC   (1<<3)  
#define PORT_LSDA   (1<<8)  
#define PORT_RESET  (1<<9)  
#define PORT_SUSP   (1<<12)

struct alignas(16) TD {
    volatile uint32_t link;
    volatile uint32_t ctrl_sts;
    volatile uint32_t token;
    volatile uint32_t buf_ptr;
    uint32_t _pad[4];
};

#define TD_LINK_TERM    (1<<0)
#define TD_LINK_QH      (1<<1)
#define TD_LINK_DEPTH   (1<<2)

#define TD_CS_ACTIVE        (1<<23)
#define TD_CS_STALLED       (1<<22)
#define TD_CS_DBUF_ERR      (1<<21)
#define TD_CS_BABBLE        (1<<20)
#define TD_CS_NAK           (1<<19)
#define TD_CS_CRC_TIMEOUT   (1<<18)
#define TD_CS_BITSTUFF      (1<<17)
#define TD_CS_IOC           (1<<24)
#define TD_CS_ISO           (1<<25)
#define TD_CS_LS            (1<<26)
#define TD_CS_SPD           (1<<29)
#define TD_CS_ERR_SHIFT     27
#define TD_CS_ACTLEN_MASK   0x7FF

#define TD_TOKEN_PID_SETUP  0x2D
#define TD_TOKEN_PID_IN     0x69
#define TD_TOKEN_PID_OUT    0xE1
#define TD_TOKEN_TOGGLE(t)  ((uint32_t)(t) << 19)
#define TD_TOKEN_MAXLEN(n)  ((uint32_t)((n) - 1) << 21)
#define TD_TOKEN_DEVADDR(a) ((uint32_t)(a) << 8)
#define TD_TOKEN_ENDP(e)    ((uint32_t)(e) << 15)

struct alignas(16) QH {
    volatile uint32_t head_link;
    volatile uint32_t elem_link;
    uint32_t _pad[2];
};

#define QH_LINK_TERM  (1<<0)
#define QH_LINK_QH    (1<<1)

#define FRAME_COUNT  1024

static uint32_t* s_frame_list = nullptr;
static QH*       s_int_qh     = nullptr;
static QH*       s_ctrl_qh    = nullptr;
static TD*       s_td_pool    = nullptr;
static uint8_t*  s_buf_pool   = nullptr;

static uint16_t  s_iobase = 0;
static uint8_t   s_irq    = 0;

struct IntPipe {
    bool       active;
    uint8_t    dev_addr;
    uint8_t    endpoint;
    uint8_t    max_packet;
    uint8_t    toggle;
    TD*        td;
    uint8_t*   buf;
    UhciPollCb cb;
};

static IntPipe s_pipes[UHCI_TD_COUNT];
static int     s_pipe_count = 0;

static inline uint32_t phys(const void* p) {
    return (uint32_t)(uintptr_t)p;
}

static void uhci_reset() {
    outw(s_iobase + UHCI_USBCMD, CMD_HCRESET);
    for (int i = 0; i < 1000; i++) {
        io_delay();
        if (!(inw(s_iobase + UHCI_USBCMD) & CMD_HCRESET)) break;
    }
    outw(s_iobase + UHCI_USBCMD,  0);
    outw(s_iobase + UHCI_USBINTR, 0);
    outw(s_iobase + UHCI_USBSTS,  0x3F);
    outb(s_iobase + UHCI_SOFMOD,  0x40);
}

static void uhci_start() {
    outl(s_iobase + UHCI_FRBASEADD, phys(s_frame_list));
    outw(s_iobase + UHCI_FRNUM,     0);
    outw(s_iobase + UHCI_USBINTR,   0x0F);
    outw(s_iobase + UHCI_USBCMD,    CMD_RS | CMD_MAXP);
}

static void port_reset(uint16_t portsc_reg) {
    outw(s_iobase + portsc_reg, PORT_RESET);
    for (int i = 0; i < 50000; i++) io_delay();
    outw(s_iobase + portsc_reg, 0);
    for (int i = 0; i < 10000; i++) io_delay();
    outw(s_iobase + portsc_reg, PORT_PED | PORT_CSC | PORT_PEDC);
    for (int i = 0; i < 10000; i++) io_delay();
}

static void build_frame_list() {
    uint32_t int_qh_ptr = phys(s_int_qh) | QH_LINK_QH;
    for (int i = 0; i < FRAME_COUNT; i++)
        s_frame_list[i] = int_qh_ptr;

    s_int_qh->head_link = phys(s_ctrl_qh) | QH_LINK_QH;
    s_int_qh->elem_link = QH_LINK_TERM;

    s_ctrl_qh->head_link = QH_LINK_TERM;
    s_ctrl_qh->elem_link = QH_LINK_TERM;
}

bool uhci_init() {
    PciDevice pci = pci_find_class(0x0C, 0x03, 0x00);
    if (!pci.found) {
        terminal_puts("[UHCI] Not found on PCI\n");
        return false;
    }
    pci_enable(pci);

    s_iobase = (uint16_t)pci_bar_addr(pci, 4);
    s_irq    = pci_irq_line(pci);

    terminal_puts("[UHCI] Found, iobase=0x");
    char hex[5];
    for (int i = 3; i >= 0; i--) {
        uint8_t n = (s_iobase >> (i*4)) & 0xF;
        hex[3-i] = n < 10 ? '0'+n : 'A'+n-10;
    }
    hex[4] = 0;
    terminal_puts(hex);
    terminal_puts(" irq=");
    char irqbuf[4];
    irqbuf[0] = '0' + s_irq / 10;
    irqbuf[1] = '0' + s_irq % 10;
    irqbuf[2] = '\n'; irqbuf[3] = 0;
    terminal_puts(irqbuf);

    s_frame_list = (uint32_t*)kmalloc_aligned(4096, 4096);
    s_int_qh     = (QH*)      kmalloc_aligned(sizeof(QH) * UHCI_QH_COUNT, 16);
    s_ctrl_qh    = s_int_qh + 1;
    s_td_pool    = (TD*)      kmalloc_aligned(sizeof(TD)  * UHCI_TD_COUNT, 16);
    s_buf_pool   = (uint8_t*) kmalloc_aligned(64 * UHCI_TD_COUNT, 64);

    if (!s_frame_list || !s_int_qh || !s_td_pool || !s_buf_pool) {
        terminal_puts("[UHCI] Out of memory\n");
        return false;
    }

    for (int i = 0; i < FRAME_COUNT; i++) s_frame_list[i] = 0;
    for (int i = 0; i < UHCI_QH_COUNT; i++) {
        s_int_qh[i].head_link = QH_LINK_TERM;
        s_int_qh[i].elem_link = QH_LINK_TERM;
    }
    for (int i = 0; i < UHCI_TD_COUNT; i++) {
        TD& td = s_td_pool[i];
        td.link = td.ctrl_sts = td.token = td.buf_ptr = 0;
    }
    for (int i = 0; i < UHCI_TD_COUNT; i++) s_pipes[i] = {};

    uhci_reset();
    build_frame_list();
    uhci_start();

    port_reset(UHCI_PORTSC0);
    port_reset(UHCI_PORTSC1);

    terminal_puts("[UHCI] Running\n");
    return true;
}

struct SetupPacket {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));

static bool wait_td(TD* td, uint32_t timeout_ms) {
    for (uint32_t i = 0; i < timeout_ms * 1000; i++) {
        io_delay();
        if (!(td->ctrl_sts & TD_CS_ACTIVE)) return true;
    }
    return false;  // timeout
}

bool uhci_control(uint8_t addr, uint8_t bmRequestType, uint8_t bRequest,
                  uint16_t wValue, uint16_t wIndex, uint16_t wLength, void* data)
{
    static SetupPacket setup_buf;
    static uint8_t     status_buf[4];

    setup_buf.bmRequestType = bmRequestType;
    setup_buf.bRequest      = bRequest;
    setup_buf.wValue        = wValue;
    setup_buf.wIndex        = wIndex;
    setup_buf.wLength       = wLength;

    TD* td_setup  = &s_td_pool[UHCI_TD_COUNT - 3];
    TD* td_data   = &s_td_pool[UHCI_TD_COUNT - 2];
    TD* td_status = &s_td_pool[UHCI_TD_COUNT - 1];

    bool data_in = (bmRequestType & 0x80) != 0;

    td_setup->buf_ptr  = phys(&setup_buf);
    td_setup->token    = TD_TOKEN_PID_SETUP
                       | TD_TOKEN_DEVADDR(addr)
                       | TD_TOKEN_ENDP(0)
                       | TD_TOKEN_TOGGLE(0)
                       | TD_TOKEN_MAXLEN(8);
    td_setup->ctrl_sts = TD_CS_ACTIVE | (3u << TD_CS_ERR_SHIFT);
    td_setup->link     = wLength ? (phys(td_data) | TD_LINK_DEPTH) : (phys(td_status) | TD_LINK_DEPTH);

    if (wLength && data) {
        td_data->buf_ptr  = phys(data);
        td_data->token    = (data_in ? TD_TOKEN_PID_IN : TD_TOKEN_PID_OUT)
                          | TD_TOKEN_DEVADDR(addr)
                          | TD_TOKEN_ENDP(0)
                          | TD_TOKEN_TOGGLE(1)
                          | TD_TOKEN_MAXLEN(wLength);
        td_data->ctrl_sts = TD_CS_ACTIVE | (3u << TD_CS_ERR_SHIFT);
        td_data->link     = phys(td_status) | TD_LINK_DEPTH;
    }

    td_status->buf_ptr  = phys(status_buf);
    td_status->token    = (data_in ? TD_TOKEN_PID_OUT : TD_TOKEN_PID_IN)
                        | TD_TOKEN_DEVADDR(addr)
                        | TD_TOKEN_ENDP(0)
                        | TD_TOKEN_TOGGLE(1)
                        | TD_TOKEN_MAXLEN(0);
    td_status->ctrl_sts = TD_CS_ACTIVE | TD_CS_IOC | (3u << TD_CS_ERR_SHIFT);
    td_status->link     = TD_LINK_TERM;

    s_ctrl_qh->elem_link = phys(td_setup) | 0;

    if (!wait_td(td_status, 500)) {
        terminal_puts("[UHCI] control timeout\n");
        s_ctrl_qh->elem_link = QH_LINK_TERM;
        return false;
    }

    s_ctrl_qh->elem_link = QH_LINK_TERM;

    if (td_status->ctrl_sts & (TD_CS_STALLED | TD_CS_DBUF_ERR | TD_CS_BABBLE)) {
        terminal_puts("[UHCI] control error\n");
        return false;
    }
    return true;
}

bool uhci_set_address(uint8_t new_addr) {
    return uhci_control(0, 0x00, 5, new_addr, 0, 0, nullptr);
}

bool uhci_get_descriptor(uint8_t addr, uint8_t type, uint8_t idx,
                         void* buf, uint16_t len) {
    return uhci_control(addr, 0x80, 6, (uint16_t)((type<<8)|idx), 0, len, buf);
}

bool uhci_set_configuration(uint8_t addr, uint8_t config_val) {
    return uhci_control(addr, 0x00, 9, config_val, 0, 0, nullptr);
}

bool uhci_hid_set_boot_protocol(uint8_t addr, uint8_t iface) {
    return uhci_control(addr, 0x21, 0x0B, 0, iface, 0, nullptr);
}

bool uhci_hid_set_idle(uint8_t addr, uint8_t iface) {
    return uhci_control(addr, 0x21, 0x0A, 0, iface, 0, nullptr);
}

uint8_t uhci_register_interrupt(uint8_t addr, uint8_t endpoint,
                                uint8_t max_packet, uint8_t interval,
                                UhciPollCb cb)
{
    if (s_pipe_count >= UHCI_TD_COUNT - 3) return 0;  // -3 резерв под control

    int idx    = s_pipe_count++;
    IntPipe& p = s_pipes[idx];
    p.active     = true;
    p.dev_addr   = addr;
    p.endpoint   = endpoint;
    p.max_packet = max_packet;
    p.toggle     = 0;
    p.cb         = cb;
    p.td         = &s_td_pool[idx];
    p.buf        = &s_buf_pool[idx * 64];

    TD* td = p.td;
    td->buf_ptr  = phys(p.buf);
    td->token    = TD_TOKEN_PID_IN
                 | TD_TOKEN_DEVADDR(addr)
                 | TD_TOKEN_ENDP(endpoint)
                 | TD_TOKEN_TOGGLE(0)
                 | TD_TOKEN_MAXLEN(max_packet);
    td->ctrl_sts = TD_CS_ACTIVE | TD_CS_IOC | TD_CS_SPD | (3u << TD_CS_ERR_SHIFT);
    td->link     = TD_LINK_TERM;

    if (s_int_qh->elem_link & QH_LINK_TERM) {
        s_int_qh->elem_link = phys(td);
    } else {
        uint32_t ptr = s_int_qh->elem_link & ~0xFu;
        TD* cur = (TD*)(uintptr_t)ptr;
        while (!(cur->link & TD_LINK_TERM))
            cur = (TD*)(uintptr_t)(cur->link & ~0xFu);
        cur->link = phys(td) | TD_LINK_DEPTH;
    }

    return (uint8_t)(idx + 1);
}

void uhci_irq_handler() {
    uint16_t sts = inw(s_iobase + UHCI_USBSTS);
    if (!sts) return;
    outw(s_iobase + UHCI_USBSTS, sts);

    if (sts & STS_ERROR) {
        terminal_puts("[UHCI] error\n");
    }

    if (sts & STS_USBINT) {
        for (int i = 0; i < s_pipe_count; i++) {
            IntPipe& p = s_pipes[i];
            if (!p.active) continue;
            TD* td = p.td;

            if (td->ctrl_sts & TD_CS_ACTIVE) continue;

            if (!(td->ctrl_sts & (TD_CS_STALLED | TD_CS_DBUF_ERR | TD_CS_BABBLE))) {
                uint8_t actual = (td->ctrl_sts & TD_CS_ACTLEN_MASK) + 1;
                if (actual && p.cb)
                    p.cb(p.buf, actual);
            }

            p.toggle ^= 1;
            td->token = TD_TOKEN_PID_IN
                      | TD_TOKEN_DEVADDR(p.dev_addr)
                      | TD_TOKEN_ENDP(p.endpoint)
                      | TD_TOKEN_TOGGLE(p.toggle)
                      | TD_TOKEN_MAXLEN(p.max_packet);
            td->ctrl_sts = TD_CS_ACTIVE | TD_CS_IOC | TD_CS_SPD | (3u << TD_CS_ERR_SHIFT);
        }
    }
}