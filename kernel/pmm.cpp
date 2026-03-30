#include "pmm.h"

extern uint32_t kernel_end;

static uint8_t  bitmap[1024 * 32];
static uint32_t total_pages  = 0;
static uint32_t used_pages   = 0;
static uint32_t bitmap_size  = 0;

static void bitmap_set(uint32_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static void bitmap_clear(uint32_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static bool bitmap_test(uint32_t page) {
    return bitmap[page / 8] & (1 << (page % 8));
}

void pmm_init(uint32_t mem_size_bytes) {
    total_pages = mem_size_bytes / PAGE_SIZE;
    bitmap_size = total_pages / 8;

    for (uint32_t i = 0; i < bitmap_size; i++)
        bitmap[i] = 0xFF;
    used_pages = total_pages;

    uint32_t kernel_end_addr = (uint32_t)&kernel_end;
    uint32_t first_free = (kernel_end_addr + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t i = first_free; i < total_pages; i++) {
        bitmap_clear(i);
        used_pages--;
    }

    for (uint32_t i = 0; i < 256; i++) {
        bitmap_set(i);
    }
}

void *pmm_alloc() {
    for (uint32_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return nullptr;
}

void pmm_free(void *addr) {
    uint32_t page = (uint32_t)addr / PAGE_SIZE;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}

uint32_t pmm_free_pages() { return total_pages - used_pages; }
uint32_t pmm_used_pages() { return used_pages; }