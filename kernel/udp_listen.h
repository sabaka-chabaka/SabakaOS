#pragma once
#include <stdint.h>

#define UDPL_BUF_DEPTH   16
#define UDPL_PKT_MAX     512

struct UdpPacket {
    uint32_t src_ip;
    uint16_t src_port;
    uint8_t  data[UDPL_PKT_MAX];
    uint16_t len;
};

int  udpl_open(uint16_t port);
int  udpl_recv(int handle, UdpPacket* out);
int  udpl_recv_wait(int handle, UdpPacket* out, uint32_t timeout_ms);
void udpl_close(int handle);