#pragma once
#include <stdint.h>

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

struct __attribute__((packed)) TcpHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack_seq;
    uint8_t data_off;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
};

enum TcpState : uint8_t {
    TCP_CLOSED = 0,
    TCP_SYN_SENT,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
};

#define TCP_MAX_SOCKETS 8
#define TCP_RECV_BUF 8192
#define TCP_RETX_BUF 2048
#define TCP_MSS 1460
#define TCP_WINDOW TCP_RECV_BUF
#define TCP_RETX_TIMEOUT_MS 1000
#define TCP_MAX_RETX 6
#define TCP_TIMEWAIT_MS 4000

struct TcpSocket {
    TcpState state;

    uint8_t   retx_count;
    uint16_t  local_port;
    uint16_t  remote_port;
    uint32_t  remote_ip;

    uint32_t  snd_nxt;
    uint32_t  snd_una;
    uint32_t  rcv_nxt;

    uint8_t   retx_buf[TCP_RETX_BUF];
    uint16_t  retx_len;
    uint8_t   retx_flags;
    uint32_t  retx_seq;
    uint32_t  retx_deadline;

    uint8_t   recv_buf[TCP_RECV_BUF];
    uint32_t  recv_head;
    uint32_t  recv_tail;
    uint32_t  recv_len;

    uint32_t  timewait_deadline;

    bool      used;
};

void tcp_init();
int tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms);

int      tcp_send(int sock, const uint8_t* data, uint16_t len);
int      tcp_recv(int sock, uint8_t* buf, uint16_t maxlen);
int      tcp_recv_wait(int sock, uint8_t* buf, uint16_t maxlen, uint32_t timeout_ms);

void     tcp_close(int sock);
TcpState tcp_state(int sock);

void     tcp_receive(uint32_t src_ip, const uint8_t* seg, uint16_t len);

void     tcp_tick();