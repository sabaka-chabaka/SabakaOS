#pragma once
#include <stdint.h>

#define NET_QUEUE_DEPTH 32
#define NET_QUEUE_PKT_MAX 1536

struct NetPacket {
    uint8_t data[NET_QUEUE_PKT_MAX];
    uint16_t len;
};

struct NetQueue {
    NetPacket slots[NET_QUEUE_DEPTH];
    volatile uint32_t head;
    volatile uint32_t tail;
};

void netq_init(NetQueue* q);
bool netq_enqueue(NetQueue* q, const uint8_t* data, uint16_t len);
bool netq_dequeue(NetQueue* q, NetPacket* out);
bool netq_empty(const NetQueue* q);