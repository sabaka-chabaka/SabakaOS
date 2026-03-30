#pragma once
#include <stdint.h>

#define PAGE_PRESENT  0x1
#define PAGE_WRITE    0x2
#define PAGE_USER     0x4

void  paging_init();
void  paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
void  paging_unmap(uint32_t virt);

void  paging_map_region(uint32_t virt, uint32_t phys,
                        uint32_t size, uint32_t flags);

bool  paging_alloc_region(uint32_t virt, uint32_t size, uint32_t flags);

void* paging_get_physaddr(uint32_t virt);
bool  paging_is_mapped(uint32_t virt);