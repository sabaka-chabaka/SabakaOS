#pragma once
#include <stdint.h>

#define PAGE_SIZE 4096

void pmm_init(uint32_t mem_size_bytes);
void* pmm_alloc();
void pmm_free(void* addr);
uint32_t pmm_free_pages();
uint32_t pmm_used_pages();