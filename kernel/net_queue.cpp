#include "net_queue.h"

#include "kstring.h"

void netq_init(NetQueue *q) {
    q->head = 0;
    q->tail = 0;
}

bool netq_enqueue(NetQueue *q, const uint8_t *data, uint16_t len) {
    if (len == 0 || len > NET_QUEUE_PKT_MAX) return false;

    uint32_t head = q->head;
    uint32_t next = (head + 1) % (NET_QUEUE_DEPTH - 1);

    if (next == q->tail) return false;

    kmemcpy(q->slots[head].data, data, len);
    q->slots[head].len = len;

    __asm__ volatile("" ::: "memory");
    q->head = next;
    return true;
}

bool netq_dequeue(NetQueue *q, NetPacket *out) {
    if (q->head == q->tail) return false;

    uint32_t tail = q->tail;
    out->len = q->slots[tail].len;
    kmemcpy(out->data, q->slots[tail].data, out->len);

    __asm__ volatile("" ::: "memory");
    q->tail = (tail + 1) % (NET_QUEUE_DEPTH - 1);
    return true;
}

bool netq_empty(NetQueue *q) {
    return q->head == q->tail;
}