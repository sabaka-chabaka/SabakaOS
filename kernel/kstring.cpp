#include "kstring.h"

void* kmemset(void* dst, uint8_t val, uint32_t n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = val;
    return dst;
}

void* kmemcpy(void* dst, const void* src, uint32_t n) {
    uint8_t*       d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dst;
}

void* kmemmove(void* dst, const void* src, uint32_t n) {
    uint8_t*       d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int kmemcmp(const void* a, const void* b, uint32_t n) {
    const uint8_t* p = (const uint8_t*)a;
    const uint8_t* q = (const uint8_t*)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

uint32_t kstrlen(const char* s) {
    uint32_t n = 0;
    while (*s++) n++;
    return n;
}

int kstrcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char* a, const char* b, uint32_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char* kstrcpy(char* dst, const char* src) {
    char* r = dst;
    while ((*dst++ = *src++));
    return r;
}

char* kstrncpy(char* dst, const char* src, uint32_t n) {
    char* r = dst;
    while (n && (*dst++ = *src++)) n--;
    while (n--) *dst++ = '\0';
    return r;
}

char* kstrcat(char* dst, const char* src) {
    char* r = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return r;
}

const char* kstrchr(const char* s, char c) {
    while (*s) { if (*s == c) return s; s++; }
    return nullptr;
}

const char* kstrstr(const char* h, const char* n) {
    if (!*n) return h;
    while (*h) {
        const char *hp = h, *np = n;
        while (*hp && *np && *hp == *np) { hp++; np++; }
        if (!*np) return h;
        h++;
    }
    return nullptr;
}

static const char HEX[] = "0123456789ABCDEF";

char* kuitoa(uint32_t n, char* buf, int base) {
    if (base < 2 || base > 16) { buf[0]='?'; buf[1]=0; return buf; }
    if (n == 0) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[34]; int len = 0;
    while (n > 0) { tmp[len++] = HEX[n % base]; n /= base; }
    for (int i = 0; i < len; i++) buf[i] = tmp[len-1-i];
    buf[len] = 0;
    return buf;
}

char* kitoa(int32_t n, char* buf, int base) {
    if (n < 0 && base == 10) {
        buf[0] = '-';
        kuitoa((uint32_t)(-n), buf+1, base);
    } else {
        kuitoa((uint32_t)n, buf, base);
    }
    return buf;
}

int32_t katoi(const char* s) {
    int32_t r = 0; bool neg = false;
    if (*s == '-') { neg = true; s++; }
    while (*s >= '0' && *s <= '9') { r = r*10 + (*s-'0'); s++; }
    return neg ? -r : r;
}