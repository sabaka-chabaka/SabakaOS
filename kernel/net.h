#pragma once
#include <stdint.h>

#define ETH_ALEN        6
#define ETH_TYPE_IP     0x0800
#define ETH_TYPE_ARP    0x0806

struct __attribute__((packed)) EthHeader {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;
};

#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

struct __attribute__((packed)) ArpPacket {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_size;
    uint8_t  proto_size;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
};

#define IP_PROTO_ICMP   1
#define IP_PROTO_UDP    17
#define IP_PROTO_TCP    6

struct __attribute__((packed)) IpHeader {
    uint8_t  ver_ihl;
    uint8_t  dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
};

#define ICMP_ECHO_REQUEST  8
#define ICMP_ECHO_REPLY    0

struct __attribute__((packed)) IcmpHeader {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};

struct __attribute__((packed)) UdpHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
};

#define ARP_CACHE_SIZE 16

struct ArpEntry {
    uint32_t ip;
    uint8_t  mac[6];
    bool     valid;
};

typedef void (*udp_rx_callback)(uint32_t src_ip, uint16_t src_port,
                                 uint16_t dst_port,
                                 const uint8_t* data, uint16_t len);

void net_init(uint32_t my_ip, uint32_t gateway_ip, uint32_t netmask);

uint32_t net_get_ip();
void     net_get_mac(uint8_t mac[6]);

void net_receive(const uint8_t* data, uint16_t len);
void net_poll();

void net_arp_request(uint32_t target_ip);
bool net_arp_lookup(uint32_t ip, uint8_t mac_out[6]);

bool net_udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                  const uint8_t* data, uint16_t len);
void net_udp_register(uint16_t port, udp_rx_callback cb);

uint32_t ip_from_str(const char* s);
void     ip_to_str(uint32_t ip, char* out);

static inline uint16_t htons(uint16_t v){ return (uint16_t)((v>>8)|(v<<8)); }
static inline uint32_t htonl(uint32_t v){
    return ((v>>24)&0xFF)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24)&0xFF000000u);
}
static inline uint16_t ntohs(uint16_t v){ return htons(v); }
static inline uint32_t ntohl(uint32_t v){ return htonl(v); }