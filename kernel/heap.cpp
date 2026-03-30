#include "heap.h"

struct BlockHeader {
    uint32_t    size;
    bool        free;
    BlockHeader* next;
    BlockHeader* prev;
    uint32_t    magic;
};

#define HEAP_MAGIC 0xABCD1234
#define MIN_SPLIT  32

static BlockHeader* heap_start = nullptr;
static uint32_t     heap_base  = 0;
static uint32_t     heap_limit = 0;
static uint32_t     heap_current = 0;
static uint32_t     used_bytes = 0;

void heap_init(uint32_t start, uint32_t size) {
    heap_base    = start;
    heap_limit   = start + size;
    heap_current = start;
    used_bytes   = 0;

    heap_start = (BlockHeader*)heap_base;
    heap_start->size  = size - sizeof(BlockHeader);
    heap_start->free  = true;
    heap_start->next  = nullptr;
    heap_start->prev  = nullptr;
    heap_start->magic = HEAP_MAGIC;
}

static BlockHeader* find_free(uint32_t size) {
    BlockHeader* b = heap_start;
    while (b) {
        if (b->free && b->size >= size && b->magic == HEAP_MAGIC)
            return b;
        b = b->next;
    }
    return nullptr;
}

static void split(BlockHeader* b, uint32_t size) {
    if (b->size < size + sizeof(BlockHeader) + MIN_SPLIT) return;

    BlockHeader* newb = (BlockHeader*)((uint8_t*)b + sizeof(BlockHeader) + size);
    newb->size  = b->size - size - sizeof(BlockHeader);
    newb->free  = true;
    newb->next  = b->next;
    newb->prev  = b;
    newb->magic = HEAP_MAGIC;

    if (b->next) b->next->prev = newb;
    b->next = newb;
    b->size = size;
}

void* kmalloc(uint32_t size) {
    if (!size) return nullptr;
    size = (size + 7) & ~7;

    BlockHeader* b = find_free(size);
    if (!b) return nullptr;

    split(b, size);
    b->free = false;
    used_bytes += size;
    return (void*)((uint8_t*)b + sizeof(BlockHeader));
}

void* kmalloc_aligned(uint32_t size, uint32_t align) {
    uint8_t* raw = (uint8_t*)kmalloc(size + align);
    if (!raw) return nullptr;
    uint32_t addr = (uint32_t)raw;
    uint32_t aligned = (addr + align - 1) & ~(align - 1);
    return (void*)aligned;
}

void kfree(void* ptr) {
    if (!ptr) return;

    BlockHeader* b = (BlockHeader*)((uint8_t*)ptr - sizeof(BlockHeader));
    if (b->magic != HEAP_MAGIC) return;
    if (b->free) return;

    b->free = true;
    if (used_bytes >= b->size) used_bytes -= b->size;

    if (b->next && b->next->free && b->next->magic == HEAP_MAGIC) {
        b->size += sizeof(BlockHeader) + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }

    if (b->prev && b->prev->free && b->prev->magic == HEAP_MAGIC) {
        b->prev->size += sizeof(BlockHeader) + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

uint32_t heap_used() { return used_bytes; }
uint32_t heap_free() {
    uint32_t f = 0;
    BlockHeader* b = heap_start;
    while (b) { if (b->free) f += b->size; b = b->next; }
    return f;
}

void* operator new(uint32_t size)             { return kmalloc(size); }
void* operator new[](uint32_t size)           { return kmalloc(size); }
void  operator delete(void* p) noexcept       { kfree(p); }
void  operator delete[](void* p) noexcept     { kfree(p); }
void  operator delete(void* p, uint32_t) noexcept  { kfree(p); }
void  operator delete[](void* p, uint32_t) noexcept { kfree(p); }