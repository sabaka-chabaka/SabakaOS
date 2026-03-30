#pragma once
#include <stdint.h>

void  heap_init(uint32_t start, uint32_t size);
void* kmalloc(uint32_t size);
void* kmalloc_aligned(uint32_t size, uint32_t align);
void  kfree(void* ptr);

uint32_t heap_used();
uint32_t heap_free();

void* operator new(uint32_t size);
void* operator new[](uint32_t size);
void  operator delete(void* ptr) noexcept;
void  operator delete[](void* ptr) noexcept;
void  operator delete(void* ptr, uint32_t) noexcept;
void  operator delete[](void* ptr, uint32_t) noexcept;