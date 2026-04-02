#include "net.h"
#include "rtl8139.h"
#include "heap.h"
#include "kstring.h"
#include "terminal.h"

static uint32_t s_my_ip      = 0;
static uint32_t s_gateway_ip = 0;
static uint32_t s_netmask    = 0;
static uint8_t  s_my_mac[6]  = {};
static ArpEntry s_arp_cache[ARP_CACHE_SIZE];

#define UDP_HANDLERS 8
struct UdpHandler { uint16_t port; udp_rx_callback cb; bool used; };
static UdpHandler s_udp_handlers[UDP_HANDLERS];
static uint16_t s_ip_id = 1;

static uint16_t ip_checksum(const void* data, uint32_t len) {
    const uint16_t* p = (const uint16_t*)data;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(uint8_t*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

uint32_t ip_from_str(const char* s) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t o = 0;
        while (*s >= '0' && *s <= '9') o = o * 10 + (*s++ - '0');
        if (*s == '.') s++;
        r = (r << 8) | (o & 0xFF);
    }
    return htonl(r);
}

void ip_to_str(uint32_t ip, char* out) {
    ip = ntohl(ip);
    int pos = 0;
    for (int i = 3; i >= 0; i--) {
        uint8_t o = (uint8_t)(ip >> (i * 8));
        if (o >= 100) { out[pos++] = '0' + o/100; o %= 100; out[pos++] = '0' + o/10; o %= 10; }
        else if (o >= 10) { out[pos++] = '0' + o/10; o %= 10; }
        out[pos++] = '0' + o;
        if (i > 0) out[pos++] = '.';
    }
    out[pos] = 0;
}

static void arp_cache_store(uint32_t ip, const uint8_t* mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (s_arp_cache[i].valid && s_arp_cache[i].ip == ip) {
            kmemcpy(s_arp_cache[i].mac, mac, 6); return;
        }
    }
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!s_arp_cache[i].valid) {
            s_arp_cache[i].ip = ip;
            kmemcpy(s_arp_cache[i].mac, mac, 6);
            s_arp_cache[i].valid = true; return;
        }
    }
    s_arp_cache[0].ip = ip;
    kmemcpy(s_arp_cache[0].mac, mac, 6);
    s_arp_cache[0].valid = true;
}

bool net_arp_lookup(uint32_t ip, uint8_t mac_out[6]) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (s_arp_cache[i].valid && s_arp_cache[i].ip == ip) {
            kmemcpy(mac_out, s_arp_cache[i].mac, 6); return true;
        }
    }
    return false;
}

void net_arp_request(uint32_t target_ip) {
    uint8_t pkt[sizeof(EthHeader) + sizeof(ArpPacket)];
    kmemset(pkt, 0, sizeof(pkt));
    EthHeader* eth = (EthHeader*)pkt;
    kmemset(eth->dst, 0xFF, 6);
    kmemcpy(eth->src, s_my_mac, 6);
    eth->type = htons(ETH_TYPE_ARP);
    ArpPacket* arp = (ArpPacket*)(pkt + sizeof(EthHeader));
    arp->hw_type = htons(1); arp->proto_type = htons(0x0800);
    arp->hw_size = 6; arp->proto_size = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    kmemcpy(arp->sender_mac, s_my_mac, 6);
    arp->sender_ip = s_my_ip;
    kmemset(arp->target_mac, 0, 6);
    arp->target_ip = target_ip;
    rtl8139_send(pkt, (uint16_t)sizeof(pkt));
}

static void arp_reply(const ArpPacket* req) {
    uint8_t pkt[sizeof(EthHeader) + sizeof(ArpPacket)];
    kmemset(pkt, 0, sizeof(pkt));
    EthHeader* eth = (EthHeader*)pkt;
    kmemcpy(eth->dst, req->sender_mac, 6);
    kmemcpy(eth->src, s_my_mac, 6);
    eth->type = htons(ETH_TYPE_ARP);
    ArpPacket* arp = (ArpPacket*)(pkt + sizeof(EthHeader));
    arp->hw_type = htons(1); arp->proto_type = htons(0x0800);
    arp->hw_size = 6; arp->proto_size = 4;
    arp->opcode = htons(ARP_OP_REPLY);
    kmemcpy(arp->sender_mac, s_my_mac, 6);
    arp->sender_ip = s_my_ip;
    kmemcpy(arp->target_mac, req->sender_mac, 6);
    arp->target_ip = req->sender_ip;
    rtl8139_send(pkt, (uint16_t)sizeof(pkt));
}

static void handle_arp(const uint8_t* data, uint16_t len) {
    if (len < (uint16_t)sizeof(ArpPacket)) return;
    const ArpPacket* arp = (const ArpPacket*)data;
    arp_cache_store(arp->sender_ip, arp->sender_mac);
    if (ntohs(arp->opcode) == ARP_OP_REQUEST && arp->target_ip == s_my_ip)
        arp_reply(arp);
}

static void handle_icmp(const IpHeader* ip_hdr, const uint8_t* data, uint16_t len) {
    if (len < (uint16_t)sizeof(IcmpHeader)) return;
    const IcmpHeader* icmp = (const IcmpHeader*)data;
    if (icmp->type != ICMP_ECHO_REQUEST) return;
    uint16_t ip_total = (uint16_t)(sizeof(IpHeader) + len);
    uint16_t pkt_size = (uint16_t)(sizeof(EthHeader) + ip_total);
    uint8_t* pkt = (uint8_t*)kmalloc(pkt_size);
    if (!pkt) return;
    kmemset(pkt, 0, pkt_size);
    EthHeader* eth = (EthHeader*)pkt;
    if (!net_arp_lookup(ip_hdr->src_ip, eth->dst)) kmemset(eth->dst, 0xFF, 6);
    kmemcpy(eth->src, s_my_mac, 6);
    eth->type = htons(ETH_TYPE_IP);
    IpHeader* rip = (IpHeader*)(pkt + sizeof(EthHeader));
    rip->ver_ihl = 0x45; rip->dscp_ecn = 0;
    rip->total_len = htons(ip_total); rip->id = htons(s_ip_id++);
    rip->flags_frag = 0; rip->ttl = 64; rip->protocol = IP_PROTO_ICMP;
    rip->checksum = 0; rip->src_ip = s_my_ip; rip->dst_ip = ip_hdr->src_ip;
    rip->checksum = ip_checksum(rip, sizeof(IpHeader));
    IcmpHeader* ricmp = (IcmpHeader*)(pkt + sizeof(EthHeader) + sizeof(IpHeader));
    ricmp->type = ICMP_ECHO_REPLY; ricmp->code = 0; ricmp->checksum = 0;
    ricmp->id = icmp->id; ricmp->seq = icmp->seq;
    uint16_t pl = (uint16_t)(len - sizeof(IcmpHeader));
    if (pl > 0) kmemcpy((uint8_t*)ricmp + sizeof(IcmpHeader), data + sizeof(IcmpHeader), pl);
    ricmp->checksum = ip_checksum(ricmp, len);
    rtl8139_send(pkt, pkt_size);
    kfree(pkt);
}

static void handle_udp(const IpHeader* ip_hdr, const uint8_t* data, uint16_t len) {
    if (len < (uint16_t)sizeof(UdpHeader)) return;
    const UdpHeader* udp = (const UdpHeader*)data;
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t src_port = ntohs(udp->src_port);
    const uint8_t* payload = data + sizeof(UdpHeader);
    uint16_t plen = (uint16_t)(ntohs(udp->length) - sizeof(UdpHeader));
    for (int i = 0; i < UDP_HANDLERS; i++) {
        if (s_udp_handlers[i].used && s_udp_handlers[i].port == dst_port) {
            s_udp_handlers[i].cb(ip_hdr->src_ip, src_port, dst_port, payload, plen);
            return;
        }
    }
}

bool net_udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                  const uint8_t* data, uint16_t len) {
    uint16_t udp_len  = (uint16_t)(sizeof(UdpHeader) + len);
    uint16_t ip_total = (uint16_t)(sizeof(IpHeader) + udp_len);
    uint16_t pkt_size = (uint16_t)(sizeof(EthHeader) + ip_total);
    uint8_t* pkt = (uint8_t*)kmalloc(pkt_size);
    if (!pkt) return false;
    kmemset(pkt, 0, pkt_size);

    uint32_t next_hop = dst_ip;
    if ((dst_ip & s_netmask) != (s_my_ip & s_netmask))
        next_hop = s_gateway_ip;

    EthHeader* eth = (EthHeader*)pkt;
    uint8_t dst_mac[6];
    if (!net_arp_lookup(next_hop, dst_mac)) {
        // ── ФИКС: если нет ARP — шлём на broadcast.
        // QEMU SLIRP примет пакет даже на broadcast MAC.
        kmemset(dst_mac, 0xFF, 6);
    }
    kmemcpy(eth->dst, dst_mac, 6);
    kmemcpy(eth->src, s_my_mac, 6);
    eth->type = htons(ETH_TYPE_IP);

    IpHeader* ip = (IpHeader*)(pkt + sizeof(EthHeader));
    ip->ver_ihl = 0x45; ip->dscp_ecn = 0;
    ip->total_len = htons(ip_total); ip->id = htons(s_ip_id++);
    ip->flags_frag = 0; ip->ttl = 64; ip->protocol = IP_PROTO_UDP;
    ip->checksum = 0; ip->src_ip = s_my_ip; ip->dst_ip = dst_ip;
    ip->checksum = ip_checksum(ip, sizeof(IpHeader));

    UdpHeader* udp = (UdpHeader*)(pkt + sizeof(EthHeader) + sizeof(IpHeader));
    udp->src_port = htons(src_port); udp->dst_port = htons(dst_port);
    udp->length = htons(udp_len); udp->checksum = 0;
    kmemcpy((uint8_t*)udp + sizeof(UdpHeader), data, len);

    bool ok = rtl8139_send(pkt, pkt_size);
    kfree(pkt);
    return ok;
}

void net_udp_register(uint16_t port, udp_rx_callback cb) {
    for (int i = 0; i < UDP_HANDLERS; i++) {
        if (!s_udp_handlers[i].used) {
            s_udp_handlers[i] = { port, cb, true }; return;
        }
    }
}

static void handle_ip(const uint8_t* data, uint16_t len) {
    if (len < (uint16_t)sizeof(IpHeader)) return;
    const IpHeader* ip = (const IpHeader*)data;
    uint8_t ihl = (uint8_t)((ip->ver_ihl & 0x0F) * 4);
    if (ihl < 20 || ihl > len) return;
    uint32_t bcast = s_my_ip | ~s_netmask;
    if (ip->dst_ip != s_my_ip && ip->dst_ip != bcast && ip->dst_ip != 0xFFFFFFFF) return;
    const uint8_t* payload = data + ihl;
    uint16_t plen = (uint16_t)(ntohs(ip->total_len) - ihl);
    switch (ip->protocol) {
        case IP_PROTO_ICMP: handle_icmp(ip, payload, plen); break;
        case IP_PROTO_UDP:  handle_udp(ip, payload, plen);  break;
        default: break;
    }
}

void net_receive(const uint8_t* data, uint16_t len) {
    if (len < (uint16_t)sizeof(EthHeader)) return;
    const EthHeader* eth = (const EthHeader*)data;
    uint16_t type = ntohs(eth->type);
    const uint8_t* payload = data + sizeof(EthHeader);
    uint16_t plen = (uint16_t)(len - sizeof(EthHeader));
    switch (type) {
        case ETH_TYPE_ARP: handle_arp(payload, plen); break;
        case ETH_TYPE_IP:  handle_ip(payload, plen);  break;
        default: break;
    }
}

uint32_t net_get_ip() { return s_my_ip; }
void net_get_mac(uint8_t mac[6]) { kmemcpy(mac, s_my_mac, 6); }

void net_init(uint32_t my_ip, uint32_t gateway_ip, uint32_t netmask) {
    s_my_ip      = my_ip;
    s_gateway_ip = gateway_ip;
    s_netmask    = netmask;
    rtl8139_get_mac(s_my_mac);

    for (int i = 0; i < ARP_CACHE_SIZE; i++) s_arp_cache[i].valid = false;
    for (int i = 0; i < UDP_HANDLERS;   i++) s_udp_handlers[i].used = false;

    rtl8139_set_rx_callback(net_receive);

    uint8_t slirp_mac[6] = { 0x52, 0x55, 0x0A, 0x00, 0x02, 0x02 };
    arp_cache_store(gateway_ip, slirp_mac);

    uint32_t dns_ip = htonl(ntohl(gateway_ip) + 1);
    arp_cache_store(dns_ip, slirp_mac);

    char ip_str[16];
    ip_to_str(my_ip, ip_str);
    terminal_puts("[NET] IP: ");
    terminal_puts(ip_str);
    ip_to_str(gateway_ip, ip_str);
    terminal_puts("  GW: ");
    terminal_puts(ip_str);
    terminal_puts(" [52:55:0A:00:02:02]\n");
}