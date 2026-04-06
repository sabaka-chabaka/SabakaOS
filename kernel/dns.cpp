#include "dns.h"
#include "net.h"
#include "kstring.h"
#include "heap.h"
#include "terminal.h"
#include "pit.h"

struct __attribute__((packed)) DnsHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

#define DNS_FLAG_QR      (1 << 15)
#define DNS_FLAG_RD      (1 << 8)
#define DNS_FLAG_RA      (1 << 7)
#define DNS_RCODE_MASK   0x000F
#define DNS_RCODE_OK     0

static uint32_t      s_dns_server  = 0;
static uint16_t      s_next_id     = 0x1337;
static DnsCacheEntry s_cache[DNS_CACHE_SIZE];

static volatile bool     s_waiting   = false;
static volatile uint16_t s_wait_id   = 0;
static uint8_t           s_resp_buf[512];
static volatile uint16_t s_resp_len  = 0;

void dns_init(uint32_t server_ip) {
    s_dns_server = server_ip;
    s_next_id    = 0x1337;
    for (int i = 0; i < DNS_CACHE_SIZE; i++)
        s_cache[i].valid = false;
}

static DnsCacheEntry* cache_lookup(const char* name) {
    uint32_t now = pit_uptime_ms();
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!s_cache[i].valid) continue;
        if (now >= s_cache[i].ttl_deadline_ms) {
            s_cache[i].valid = false;
            continue;
        }
        if (kstrcmp(s_cache[i].name, name) == 0)
            return &s_cache[i];
    }
    return nullptr;
}

static void cache_store(const char* name, uint32_t ip, uint32_t ttl_sec) {
    uint32_t now = pit_uptime_ms();
    int slot = 0;
    uint32_t oldest_deadline = 0xFFFFFFFF;
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!s_cache[i].valid) { slot = i; break; }
        if (s_cache[i].ttl_deadline_ms < oldest_deadline) {
            oldest_deadline = s_cache[i].ttl_deadline_ms;
            slot = i;
        }
    }
    kstrncpy(s_cache[slot].name, name, DNS_MAX_NAME);
    s_cache[slot].name[DNS_MAX_NAME] = 0;
    s_cache[slot].ip              = ip;
    if (ttl_sec < 30)  ttl_sec = 30;
    if (ttl_sec > 300) ttl_sec = 300;
    s_cache[slot].ttl_deadline_ms = now + ttl_sec * 1000;
    s_cache[slot].valid           = true;
}

int dns_cache_count() {
    int n = 0;
    uint32_t now = pit_uptime_ms();
    for (int i = 0; i < DNS_CACHE_SIZE; i++)
        if (s_cache[i].valid && now < s_cache[i].ttl_deadline_ms) n++;
    return n;
}

const DnsCacheEntry* dns_cache_get(int i) {
    if (i < 0 || i >= DNS_CACHE_SIZE) return nullptr;
    return &s_cache[i];
}

void dns_cache_flush() {
    for (int i = 0; i < DNS_CACHE_SIZE; i++)
        s_cache[i].valid = false;
}

static uint16_t encode_name(const char* name, uint8_t* out) {
    uint16_t pos = 0;
    const char* p = name;
    while (*p) {
        const char* dot = kstrchr(p, '.');
        uint8_t label_len = dot ? (uint8_t)(dot - p) : (uint8_t)kstrlen(p);
        if (label_len == 0 || label_len > 63) break;
        out[pos++] = label_len;
        kmemcpy(out + pos, p, label_len);
        pos += label_len;
        p   += label_len;
        if (*p == '.') p++;
    }
    out[pos++] = 0;
    return pos;
}

static int decode_name(const uint8_t* pkt, uint16_t pkt_len,
                       uint16_t offset, char* out, uint16_t out_max) {
    uint16_t pos     = offset;
    uint16_t out_pos = 0;
    int      jumps   = 0;
    int      end_pos = -1;

    while (pos < pkt_len) {
        uint8_t len = pkt[pos];

        if (len == 0) {
            if (out_pos > 0 && out[out_pos - 1] == '.')
                out_pos--;
            out[out_pos] = 0;
            return (end_pos >= 0) ? end_pos : pos + 1;
        }

        if ((len & 0xC0) == 0xC0) {
            if (pos + 1 >= pkt_len) return -1;
            uint16_t ptr = (uint16_t)(((len & 0x3F) << 8) | pkt[pos + 1]);
            if (end_pos < 0) end_pos = pos + 2;
            pos = ptr;
            if (++jumps > 10) return -1;
            continue;
        }

        pos++;
        if (pos + len > pkt_len) return -1;
        if (out_pos + len + 1 >= out_max) return -1;
        kmemcpy(out + out_pos, pkt + pos, len);
        out_pos += len;
        out[out_pos++] = '.';
        pos += len;
    }
    return -1;
}

static void dns_udp_callback(uint32_t src_ip, uint16_t src_port,
                              uint16_t /*dst_port*/,
                              const uint8_t* data, uint16_t len) {
    (void)src_port;
    if (src_ip != s_dns_server) return;
    if (!s_waiting) return;
    if (len < (uint16_t)sizeof(DnsHeader) || len > 512) return;

    const DnsHeader* hdr = (const DnsHeader*)data;
    if (ntohs(hdr->id) != s_wait_id) return;
    if (!(ntohs(hdr->flags) & DNS_FLAG_QR)) return;

    kmemcpy(s_resp_buf, data, len);
    s_resp_len = len;

    __asm__ volatile("" ::: "memory");
    s_waiting = false;
}

static uint32_t parse_response(const uint8_t* pkt, uint16_t pkt_len) {
    if (pkt_len < (uint16_t)sizeof(DnsHeader)) return 0;

    const DnsHeader* hdr = (const DnsHeader*)pkt;
    uint16_t flags  = ntohs(hdr->flags);
    uint16_t rcode  = flags & DNS_RCODE_MASK;
    uint16_t ancount= ntohs(hdr->ancount);
    uint16_t qdcount= ntohs(hdr->qdcount);

    if (rcode != DNS_RCODE_OK) return 0;
    if (ancount == 0) return 0;

    uint16_t pos = sizeof(DnsHeader);

    for (uint16_t i = 0; i < qdcount; i++) {
        char tmp[DNS_MAX_NAME + 1];
        int next = decode_name(pkt, pkt_len, pos, tmp, sizeof(tmp));
        if (next < 0) return 0;
        pos = (uint16_t)next;
        pos += 4;
        if (pos > pkt_len) return 0;
    }

    for (uint16_t i = 0; i < ancount; i++) {
        if (pos >= pkt_len) return 0;

        char rname[DNS_MAX_NAME + 1];
        int next = decode_name(pkt, pkt_len, pos, rname, sizeof(rname));
        if (next < 0) return 0;
        pos = (uint16_t)next;

        if (pos + 10 > pkt_len) return 0;

        uint16_t rtype  = (uint16_t)((pkt[pos] << 8) | pkt[pos+1]);
        // uint16_t rclass = (uint16_t)((pkt[pos+2] << 8) | pkt[pos+3]);
        uint32_t ttl    = ((uint32_t)pkt[pos+4] << 24) | ((uint32_t)pkt[pos+5] << 16)
                        | ((uint32_t)pkt[pos+6] << 8)  |  (uint32_t)pkt[pos+7];
        uint16_t rdlen  = (uint16_t)((pkt[pos+8] << 8) | pkt[pos+9]);
        pos += 10;

        if (pos + rdlen > pkt_len) return 0;

        if (rtype == DNS_TYPE_A && rdlen == 4) {
            // Нашли A-запись
            uint32_t ip = ((uint32_t)pkt[pos]   << 24) | ((uint32_t)pkt[pos+1] << 16)
                        | ((uint32_t)pkt[pos+2] << 8)  |  (uint32_t)pkt[pos+3];
            uint32_t ip_net = htonl(ip);
            (void)ttl;
            return ip_net;
        }

        pos += rdlen;
    }
    return 0;
}

uint32_t dns_resolve(const char* hostname) {
    if (!hostname || !hostname[0]) return 0;

    {
        const char* p = hostname;
        bool looks_like_ip = true;
        int dots = 0;
        while (*p) {
            if (*p == '.') dots++;
            else if (*p < '0' || *p > '9') { looks_like_ip = false; break; }
            p++;
        }
        if (looks_like_ip && dots == 3)
            return ip_from_str(hostname);
    }

    DnsCacheEntry* cached = cache_lookup(hostname);
    if (cached) return cached->ip;

    if (!s_dns_server) return 0;

    net_udp_register(DNS_PORT, dns_udp_callback);

    uint8_t pkt[512];
    kmemset(pkt, 0, sizeof(pkt));

    uint16_t qid = s_next_id++;

    DnsHeader* hdr  = (DnsHeader*)pkt;
    hdr->id         = htons(qid);
    hdr->flags      = htons(DNS_FLAG_RD);
    hdr->qdcount    = htons(1);

    uint16_t pos = sizeof(DnsHeader);
    pos += encode_name(hostname, pkt + pos);

    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;

    uint32_t resolved = 0;

    for (int attempt = 0; attempt < DNS_MAX_RETRIES; attempt++) {
        s_resp_len = 0;
        s_wait_id  = qid;
        __asm__ volatile("" ::: "memory");
        s_waiting  = true;

        net_udp_send(s_dns_server, DNS_PORT + 1 + (uint16_t)attempt,
                     DNS_PORT, pkt, pos);

        uint32_t deadline = pit_uptime_ms() + DNS_TIMEOUT_MS;
        while (pit_uptime_ms() < deadline) {
            if (!s_waiting) break;
            __asm__ volatile("sti; hlt; cli");
        }

        if (!s_waiting && s_resp_len > 0) {
            resolved = parse_response(s_resp_buf, s_resp_len);
            if (resolved) break;
            break;
        }
    }

    s_waiting = false;

    if (resolved) {
        cache_store(hostname, resolved, 60);
    }

    return resolved;
}