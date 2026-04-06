#include "tcp.h"
#include "net.h"
#include "rtl8139.h"
#include "heap.h"
#include "kstring.h"
#include "terminal.h"
#include "pit.h"

static uint16_t chksum(const void* data, uint32_t len) {
    const uint16_t* p = (const uint16_t*)data;
    uint32_t s = 0;
    while (len > 1) { s += *p++; len -= 2; }
    if (len)         s += *(const uint8_t*)p;
    while (s >> 16)  s = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}

static uint16_t tcp_cksum(uint32_t src_ip, uint32_t dst_ip,
                           const uint8_t* seg, uint16_t seg_len) {
    struct __attribute__((packed)) {
        uint32_t src, dst;
        uint8_t  zero, proto;
        uint16_t len;
    } pseudo = { src_ip, dst_ip, 0, 6, htons(seg_len) };

    uint32_t s = 0;
    const uint16_t* p;

    uint8_t pseudo_buf[sizeof(pseudo)];
    kmemcpy(pseudo_buf, &pseudo, sizeof(pseudo));
    p = (const uint16_t*)pseudo_buf;
    for (uint32_t i = 0; i < sizeof(pseudo)/2; i++) s += p[i];

    p = (const uint16_t*)seg;
    uint16_t left = seg_len;
    while (left > 1) { s += *p++; left -= 2; }
    if (left) s += *(const uint8_t*)p;

    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}

static inline bool seq_gt(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) > 0;
}
static inline bool seq_gte(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) >= 0;
}

static TcpSocket s_socks[TCP_MAX_SOCKETS];
static uint16_t  s_next_port = 49152;

void tcp_init() {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        kmemset(&s_socks[i], 0, sizeof(TcpSocket));
        s_socks[i].state = TCP_CLOSED;
        s_socks[i].used  = false;
    }
}

static int alloc_sock() {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++)
        if (!s_socks[i].used) {
            kmemset(&s_socks[i], 0, sizeof(TcpSocket));
            s_socks[i].used = true;
            return i;
        }
    return -1;
}

static void free_sock(TcpSocket* s) {
    s->state = TCP_CLOSED;
    s->used  = false;
}

static uint16_t next_ephemeral() {
    for (int tries = 0; tries < 16384; tries++) {
        uint16_t p = s_next_port;
        s_next_port = (s_next_port >= 65535) ? 49152 : (s_next_port + 1);
        bool used = false;
        for (int i = 0; i < TCP_MAX_SOCKETS; i++)
            if (s_socks[i].used && s_socks[i].local_port == p) { used = true; break; }
        if (!used) return p;
    }
    return s_next_port++;
}

static bool tcp_send_raw(TcpSocket* s, uint8_t flags,
                          const uint8_t* payload, uint16_t pay_len,
                          bool save_retx) {
    const uint16_t hdr_len  = sizeof(TcpHeader);
    const uint16_t tcp_len  = hdr_len + pay_len;
    const uint16_t ip_total = (uint16_t)(sizeof(IpHeader) + tcp_len);
    const uint16_t pkt_sz   = (uint16_t)(sizeof(EthHeader) + ip_total);

    uint8_t* pkt = (uint8_t*)kmalloc(pkt_sz);
    if (!pkt) return false;
    kmemset(pkt, 0, pkt_sz);

    uint32_t my_ip = net_get_ip();

    EthHeader* eth = (EthHeader*)pkt;
    uint8_t dst_mac[6];
    if (!net_arp_lookup(s->remote_ip, dst_mac))
        kmemset(dst_mac, 0xFF, 6);
    kmemcpy(eth->dst, dst_mac, 6);
    net_get_mac(eth->src);
    eth->type = htons(ETH_TYPE_IP);

    static uint16_t s_ip_id = 0x6000;
    IpHeader* ip  = (IpHeader*)(pkt + sizeof(EthHeader));
    ip->ver_ihl   = 0x45;
    ip->dscp_ecn  = 0;
    ip->total_len = htons(ip_total);
    ip->id        = htons(s_ip_id++);
    ip->flags_frag= htons(0x4000);
    ip->ttl       = 64;
    ip->protocol  = IP_PROTO_TCP;
    ip->checksum  = 0;
    ip->src_ip    = my_ip;
    ip->dst_ip    = s->remote_ip;
    ip->checksum  = chksum(ip, sizeof(IpHeader));

    TcpHeader* tcp   = (TcpHeader*)(pkt + sizeof(EthHeader) + sizeof(IpHeader));
    tcp->src_port    = htons(s->local_port);
    tcp->dst_port    = htons(s->remote_port);
    tcp->seq         = htonl(s->snd_nxt);
    tcp->ack_seq     = (flags & TCP_FLAG_ACK) ? htonl(s->rcv_nxt) : 0;
    tcp->data_off    = (uint8_t)((hdr_len / 4) << 4);
    tcp->flags       = flags;
    tcp->window      = htons((uint16_t)TCP_WINDOW);
    tcp->checksum    = 0;
    tcp->urgent      = 0;

    if (pay_len > 0)
        kmemcpy((uint8_t*)tcp + hdr_len, payload, pay_len);

    tcp->checksum = tcp_cksum(my_ip, s->remote_ip, (uint8_t*)tcp, tcp_len);

    bool ok = rtl8139_send(pkt, pkt_sz);
    kfree(pkt);

    if (ok) {
        uint32_t advance = pay_len;
        if (flags & TCP_FLAG_SYN) advance++;
        if (flags & TCP_FLAG_FIN) advance++;

        if (save_retx && advance > 0) {
            s->retx_seq   = s->snd_nxt;
            s->retx_flags = flags;
            uint16_t copy = pay_len < TCP_RETX_BUF ? pay_len : TCP_RETX_BUF;
            if (payload && copy > 0) kmemcpy(s->retx_buf, payload, copy);
            s->retx_len      = copy;
            s->retx_count    = 0;
            s->retx_deadline = pit_uptime_ms() + TCP_RETX_TIMEOUT_MS;
        }

        s->snd_nxt += advance;
    }
    return ok;
}

static void tcp_send_ack(TcpSocket* s) {
    tcp_send_raw(s, TCP_FLAG_ACK, nullptr, 0, false);
}

static void tcp_send_rst_for(uint32_t dst_ip, uint16_t dst_port,
                              uint16_t src_port, uint32_t seq) {
    TcpSocket tmp;
    kmemset(&tmp, 0, sizeof(tmp));
    tmp.remote_ip   = dst_ip;
    tmp.local_port  = src_port;
    tmp.remote_port = dst_port;
    tmp.snd_nxt     = seq;
    tmp.rcv_nxt     = 0;
    tcp_send_raw(&tmp, TCP_FLAG_RST, nullptr, 0, false);
}

static void do_retransmit(TcpSocket* s) {
    if (s->retx_len == 0 && !(s->retx_flags & (TCP_FLAG_SYN|TCP_FLAG_FIN)))
        return;   // нечего ретранслировать

    s->retx_count++;
    if (s->retx_count > TCP_MAX_RETX) {
        terminal_puts("[TCP] retransmit limit, dropping connection\n");
        free_sock(s);
        return;
    }

    s->snd_nxt = s->retx_seq;
    tcp_send_raw(s, s->retx_flags,
                 s->retx_len > 0 ? s->retx_buf : nullptr,
                 s->retx_len, false);

    uint32_t next_to = TCP_RETX_TIMEOUT_MS << s->retx_count;
    if (next_to > 32000) next_to = 32000;
    s->retx_deadline = pit_uptime_ms() + next_to;
}

void tcp_tick() {
    uint32_t now = pit_uptime_ms();
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        TcpSocket* s = &s_socks[i];
        if (!s->used) continue;

        if (s->state == TCP_TIME_WAIT) {
            if (now >= s->timewait_deadline)
                free_sock(s);
            continue;
        }

        if (s->retx_deadline != 0 && now >= s->retx_deadline) {
            if (seq_gt(s->snd_nxt, s->snd_una))
                do_retransmit(s);
            else
                s->retx_deadline = 0;
        }
    }
}

void tcp_receive(uint32_t src_ip, const uint8_t* seg, uint16_t len) {
    if (len < (uint16_t)sizeof(TcpHeader)) return;

    const TcpHeader* hdr = (const TcpHeader*)seg;
    const uint8_t  hdr_bytes = (uint8_t)((hdr->data_off >> 4) * 4);
    if (hdr_bytes < 20 || hdr_bytes > len) return;

    const uint16_t src_port  = ntohs(hdr->src_port);
    const uint16_t dst_port  = ntohs(hdr->dst_port);
    const uint32_t seg_seq   = ntohl(hdr->seq);
    const uint32_t seg_ack   = ntohl(hdr->ack_seq);
    const uint8_t  flags     = hdr->flags;

    const uint8_t*  payload  = seg + hdr_bytes;
    const uint16_t  pay_len  = (uint16_t)(len - hdr_bytes);

    // Найти сокет
    TcpSocket* s = nullptr;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        TcpSocket* c = &s_socks[i];
        if (!c->used || c->state == TCP_CLOSED) continue;
        if (c->remote_ip   == src_ip   &&
            c->remote_port == src_port &&
            c->local_port  == dst_port) {
            s = c; break;
        }
    }

    if (!s) {
        if (!(flags & TCP_FLAG_RST))
            tcp_send_rst_for(src_ip, src_port, dst_port, seg_ack);
        return;
    }

    if (flags & TCP_FLAG_RST) {
        free_sock(s);
        return;
    }

    if (flags & TCP_FLAG_ACK) {
        if (seq_gt(seg_ack, s->snd_una) && seq_gte(s->snd_nxt, seg_ack)) {
            s->snd_una = seg_ack;
            if (s->snd_una == s->snd_nxt) {
                s->retx_deadline = 0;
                s->retx_len      = 0;
                s->retx_count    = 0;
            }
        }
    }

    switch (s->state) {

    case TCP_SYN_SENT:
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            if (seg_ack != s->snd_nxt) {
                tcp_send_rst_for(src_ip, src_port, dst_port, seg_ack);
                free_sock(s);
                return;
            }
            s->rcv_nxt = seg_seq + 1;
            s->snd_una = seg_ack;
            s->state   = TCP_ESTABLISHED;
            s->retx_deadline = 0;
            tcp_send_ack(s);
        }
        break;

    case TCP_ESTABLISHED:
        if (pay_len > 0 && seg_seq == s->rcv_nxt) {
            uint32_t space = TCP_RECV_BUF - s->recv_len;
            uint16_t copy  = (uint16_t)(pay_len < space ? pay_len : space);
            for (uint16_t i = 0; i < copy; i++) {
                s->recv_buf[s->recv_head] = payload[i];
                s->recv_head = (s->recv_head + 1) % TCP_RECV_BUF;
            }
            s->recv_len += copy;
            s->rcv_nxt  += copy;
            tcp_send_ack(s);
        }

        if (flags & TCP_FLAG_FIN) {
            s->rcv_nxt++;
            tcp_send_ack(s);
            s->state = TCP_LAST_ACK;
            tcp_send_raw(s, TCP_FLAG_FIN | TCP_FLAG_ACK, nullptr, 0, true);
        }
        break;

    case TCP_FIN_WAIT_1:
        if (flags & TCP_FLAG_ACK) {
            s->state = TCP_FIN_WAIT_2;
        }
        if (flags & TCP_FLAG_FIN) {
            s->rcv_nxt++;
            tcp_send_ack(s);
            s->state = TCP_TIME_WAIT;
            s->timewait_deadline = pit_uptime_ms() + TCP_TIMEWAIT_MS;
        }
        break;

    case TCP_FIN_WAIT_2:
        if (pay_len > 0 && seg_seq == s->rcv_nxt) {
            uint32_t space = TCP_RECV_BUF - s->recv_len;
            uint16_t copy  = (uint16_t)(pay_len < space ? pay_len : space);
            for (uint16_t i = 0; i < copy; i++) {
                s->recv_buf[s->recv_head] = payload[i];
                s->recv_head = (s->recv_head + 1) % TCP_RECV_BUF;
            }
            s->recv_len += copy;
            s->rcv_nxt  += copy;
        }
        if (flags & TCP_FLAG_FIN) {
            s->rcv_nxt++;
            tcp_send_ack(s);
            s->state = TCP_TIME_WAIT;
            s->timewait_deadline = pit_uptime_ms() + TCP_TIMEWAIT_MS;
        }
        break;

    case TCP_LAST_ACK:
        if (flags & TCP_FLAG_ACK) {
            free_sock(s);
        }
        break;

    case TCP_TIME_WAIT:
        if (flags & TCP_FLAG_FIN) {
            s->rcv_nxt++;
            tcp_send_ack(s);
            s->timewait_deadline = pit_uptime_ms() + TCP_TIMEWAIT_MS;
        }
        break;

    default: break;
    }
}

int tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms) {
    int id = alloc_sock();
    if (id < 0) { terminal_puts("[TCP] no free sockets\n"); return -1; }

    TcpSocket* s   = &s_socks[id];
    s->remote_ip   = dst_ip;
    s->local_port  = next_ephemeral();
    s->remote_port = dst_port;

    s->snd_nxt = pit_uptime_ms() * 7919u ^ ((uint32_t)s->local_port << 16) ^ 0xDEAD0000u;
    s->snd_una = s->snd_nxt;
    s->state   = TCP_SYN_SENT;

    if (!tcp_send_raw(s, TCP_FLAG_SYN, nullptr, 0, true)) {
        free_sock(s);
        return -1;
    }

    uint32_t deadline = pit_uptime_ms() + timeout_ms;
    while (pit_uptime_ms() < deadline) {
        if (s->state == TCP_ESTABLISHED) return id;
        if (!s->used || s->state == TCP_CLOSED) break;
        __asm__ volatile("sti; hlt; cli");
    }

    if (s->state != TCP_ESTABLISHED) {
        free_sock(s);
        return -1;
    }
    return id;
}

int tcp_send(int sock, const uint8_t* data, uint16_t len) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return -1;
    TcpSocket* s = &s_socks[sock];
    if (!s->used || s->state != TCP_ESTABLISHED) return -1;

    uint16_t sent = 0;
    while (sent < len) {
        uint16_t chunk = len - sent;
        if (chunk > TCP_MSS) chunk = TCP_MSS;

        if (!tcp_send_raw(s, TCP_FLAG_ACK | TCP_FLAG_PSH,
                          data + sent, chunk, true)) {
            return sent > 0 ? sent : -1;
        }
        sent += chunk;

        uint32_t ack_deadline = pit_uptime_ms() + TCP_RETX_TIMEOUT_MS * 3;
        while (pit_uptime_ms() < ack_deadline) {
            if (s->snd_una == s->snd_nxt) break;
            if (!s->used) return sent > 0 ? sent : -1;
            __asm__ volatile("sti; hlt; cli");
        }
    }
    return sent;
}

int tcp_recv(int sock, uint8_t* buf, uint16_t maxlen) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return -1;
    TcpSocket* s = &s_socks[sock];
    if (!s->used) return -1;

    if (s->recv_len == 0) return 0;

    uint16_t n = (uint16_t)(s->recv_len < maxlen ? s->recv_len : maxlen);
    for (uint16_t i = 0; i < n; i++) {
        buf[i] = s->recv_buf[s->recv_tail];
        s->recv_tail = (s->recv_tail + 1) % TCP_RECV_BUF;
    }
    s->recv_len -= n;
    return n;
}

int tcp_recv_wait(int sock, uint8_t* buf, uint16_t maxlen, uint32_t timeout_ms) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return -1;
    TcpSocket* s = &s_socks[sock];
    if (!s->used) return -1;

    uint32_t deadline = pit_uptime_ms() + timeout_ms;
    while (pit_uptime_ms() < deadline) {
        if (s->recv_len > 0) break;
        if (!s->used) break;
        if (s->state == TCP_CLOSE_WAIT || s->state == TCP_LAST_ACK ||
            s->state == TCP_CLOSED || s->state == TCP_TIME_WAIT) break;
        __asm__ volatile("sti; hlt; cli");
    }
    return tcp_recv(sock, buf, maxlen);
}

void tcp_close(int sock) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return;
    TcpSocket* s = &s_socks[sock];
    if (!s->used) return;

    if (s->state == TCP_ESTABLISHED) {
        s->state = TCP_FIN_WAIT_1;
        tcp_send_raw(s, TCP_FLAG_FIN | TCP_FLAG_ACK, nullptr, 0, true);

        uint32_t deadline = pit_uptime_ms() + 4000;
        while (pit_uptime_ms() < deadline) {
            if (!s->used || s->state == TCP_TIME_WAIT || s->state == TCP_CLOSED)
                break;
            __asm__ volatile("sti; hlt; cli");
        }
    }

    if (s->state != TCP_TIME_WAIT)
        free_sock(s);
}

TcpState tcp_state(int sock) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return TCP_CLOSED;
    if (!s_socks[sock].used) return TCP_CLOSED;
    return s_socks[sock].state;
}