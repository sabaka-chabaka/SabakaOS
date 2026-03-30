#pragma once
#include <stdint.h>

void*    kmemset (void* dst, uint8_t val, uint32_t n);
void*    kmemcpy (void* dst, const void* src, uint32_t n);
void*    kmemmove(void* dst, const void* src, uint32_t n);
int      kmemcmp (const void* a, const void* b, uint32_t n);

uint32_t kstrlen (const char* s);
int      kstrcmp (const char* a, const char* b);
int      kstrncmp(const char* a, const char* b, uint32_t n);
char*    kstrcpy (char* dst, const char* src);
char*    kstrncpy(char* dst, const char* src, uint32_t n);
char*    kstrcat (char* dst, const char* src);
const char* kstrchr(const char* s, char c);
const char* kstrstr(const char* haystack, const char* needle);

char*    kitoa  (int32_t  n, char* buf, int base = 10);
char*    kuitoa (uint32_t n, char* buf, int base = 10);

int32_t  katoi  (const char* s);