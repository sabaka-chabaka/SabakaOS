#include "heap.h"
#include "kstring.h"

struct Block {
    uint32_t size;
    bool     free;
    Block*   next;
    Block*   prev;
    uint32_t magic;
};

#define HEAP_MAGIC 0xABCD1234
#define MIN_SPLIT  16

static Block*   heap_head  = nullptr;
static uint32_t heap_start = 0;
static uint32_t heap_sz    = 0;
static uint32_t used_bytes = 0;

void heap_init(uint32_t start, uint32_t size) {
    heap_start = start;
    heap_sz    = size;
    used_bytes = 0;

    heap_head        = (Block*)start;
    heap_head->size  = size - sizeof(Block);
    heap_head->free  = true;
    heap_head->next  = nullptr;
    heap_head->prev  = nullptr;
    heap_head->magic = HEAP_MAGIC;
}

static Block* find_free(uint32_t size) {
    for (Block* b = heap_head; b; b = b->next)
        if (b->free && b->size >= size && b->magic == HEAP_MAGIC)
            return b;
    return nullptr;
}

static void try_split(Block* b, uint32_t size) {
    if (b->size < size + sizeof(Block) + MIN_SPLIT) return;
    Block* nb    = (Block*)((uint8_t*)b + sizeof(Block) + size);
    nb->size     = b->size - size - sizeof(Block);
    nb->free     = true;
    nb->next     = b->next;
    nb->prev     = b;
    nb->magic    = HEAP_MAGIC;
    if (b->next) b->next->prev = nb;
    b->next      = nb;
    b->size      = size;
}

static void try_merge(Block* b) {
    if (b->next && b->next->free && b->next->magic == HEAP_MAGIC) {
        b->size += sizeof(Block) + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free && b->prev->magic == HEAP_MAGIC) {
        b->prev->size += sizeof(Block) + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void* kmalloc(uint32_t size) {
    if (!size) return nullptr;
    size = (size + 7) & ~7u;
    Block* b = find_free(size);
    if (!b) return nullptr;
    try_split(b, size);
    b->free = false;
    used_bytes += b->size;
    return (void*)((uint8_t*)b + sizeof(Block));
}

void* kmalloc_aligned(uint32_t size, uint32_t align) {
    void* raw = kmalloc(size + align);
    if (!raw) return nullptr;
    uint32_t addr = (uint32_t)raw;
    return (void*)((addr + align - 1) & ~(align - 1));
}

void* krealloc(void* ptr, uint32_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (!new_size) { kfree(ptr); return nullptr; }

    Block* b = (Block*)((uint8_t*)ptr - sizeof(Block));
    if (b->magic != HEAP_MAGIC) return nullptr;

    new_size = (new_size + 7) & ~7u;

    if (b->size >= new_size) return ptr;

    void* newptr = kmalloc(new_size);
    if (!newptr) return nullptr;
    kmemcpy(newptr, ptr, b->size);
    kfree(ptr);
    return newptr;
}

void kfree(void* ptr) {
    if (!ptr) return;
    Block* b = (Block*)((uint8_t*)ptr - sizeof(Block));
    if (b->magic != HEAP_MAGIC || b->free) return;
    b->free = true;
    if (used_bytes >= b->size) used_bytes -= b->size;
    try_merge(b);
}

uint32_t heap_used()  { return used_bytes; }
uint32_t heap_total() { return heap_sz; }
uint32_t heap_free()  {
    uint32_t f = 0;
    for (Block* b = heap_head; b; b = b->next)
        if (b->free) f += b->size;
    return f;
}

void* operator new  (uint32_t s)            { return kmalloc(s); }
void* operator new[](uint32_t s)            { return kmalloc(s); }
void  operator delete  (void* p) noexcept   { kfree(p); }
void  operator delete[](void* p) noexcept   { kfree(p); }
void  operator delete  (void* p,uint32_t) noexcept { kfree(p); }
void  operator delete[](void* p,uint32_t) noexcept { kfree(p); }