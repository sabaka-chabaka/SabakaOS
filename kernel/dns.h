#pragma once
#include <stdint.h>

#define DNS_PORT 53
#define DNS_MAX_NAME 253
#define DNS_CACHE_SIZE 16
#define DNS_TIMEOUT_MS 3000
#define DNS_MAX_RETRIES 3

#define DNS_TYPE_A 1
#define DNS_TYPE_CNAME 5

struct DnsCacheEntry {
    char name[DNS_MAX_NAME + 1];
    uint32_t ip;
    uint32_t ttl_deadline_ms;
    bool valid;
};

void dns_init(uint32_t server_ip);

uint32_t dns_resolve(const char* hostname);

int dns_cache_count();
const DnsCacheEntry* dns_cache_get(int i);

void dns_cache_flush();