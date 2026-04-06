#include "udp_listen.h"
#include "net.h"
#include "kstring.h"
#include "pit.h"

struct UdpListener {
    uint16_t  port;
    bool      used;

    UdpPacket slots[UDPL_BUF_DEPTH];
    volatile uint32_t head;
    volatile uint32_t tail;
};

#define UDPL_MAX 4
static UdpListener s_listeners[UDPL_MAX];

static void make_callback(int idx,
                           uint32_t src_ip, uint16_t src_port,
                           uint16_t /*dst_port*/,
                           const uint8_t* data, uint16_t len) {
    UdpListener* l = &s_listeners[idx];
    if (!l->used) return;

    uint32_t next = (l->head + 1) & (UDPL_BUF_DEPTH - 1);
    if (next == l->tail) return;

    UdpPacket* slot = &l->slots[l->head];
    slot->src_ip   = src_ip;
    slot->src_port = src_port;
    uint16_t copy  = len < UDPL_PKT_MAX ? len : UDPL_PKT_MAX;
    kmemcpy(slot->data, data, copy);
    slot->len = copy;

    __asm__ volatile("" ::: "memory");
    l->head = next;
}

static void cb0(uint32_t a,uint16_t b,uint16_t c,const uint8_t* d,uint16_t e){make_callback(0,a,b,c,d,e);}
static void cb1(uint32_t a,uint16_t b,uint16_t c,const uint8_t* d,uint16_t e){make_callback(1,a,b,c,d,e);}
static void cb2(uint32_t a,uint16_t b,uint16_t c,const uint8_t* d,uint16_t e){make_callback(2,a,b,c,d,e);}
static void cb3(uint32_t a,uint16_t b,uint16_t c,const uint8_t* d,uint16_t e){make_callback(3,a,b,c,d,e);}

static const udp_rx_callback s_callbacks[UDPL_MAX] = { cb0, cb1, cb2, cb3 };

int udpl_open(uint16_t port) {
    for (int i = 0; i < UDPL_MAX; i++) {
        if (!s_listeners[i].used) {
            s_listeners[i].port = port;
            s_listeners[i].head = 0;
            s_listeners[i].tail = 0;
            s_listeners[i].used = true;
            net_udp_register(port, s_callbacks[i]);
            return i;
        }
    }
    return -1;
}

int udpl_recv(int handle, UdpPacket* out) {
    if (handle < 0 || handle >= UDPL_MAX) return 0;
    UdpListener* l = &s_listeners[handle];
    if (!l->used) return 0;
    if (l->head == l->tail) return 0;

    uint32_t tail = l->tail;
    *out = l->slots[tail];
    __asm__ volatile("" ::: "memory");
    l->tail = (tail + 1) & (UDPL_BUF_DEPTH - 1);
    return 1;
}

int udpl_recv_wait(int handle, UdpPacket* out, uint32_t timeout_ms) {
    if (handle < 0 || handle >= UDPL_MAX) return 0;
    uint32_t deadline = pit_uptime_ms() + timeout_ms;
    while (pit_uptime_ms() < deadline) {
        if (udpl_recv(handle, out)) return 1;
        __asm__ volatile("sti; hlt; cli");
    }
    return 0;
}

void udpl_close(int handle) {
    if (handle < 0 || handle >= UDPL_MAX) return;
    net_udp_register(s_listeners[handle].port, nullptr);
    s_listeners[handle].used = false;
}