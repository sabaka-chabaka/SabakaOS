#include "paging.h"
#include "pmm.h"

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_table_low[1024] __attribute__((aligned(4096)));

static uint32_t* get_or_create_pt(uint32_t pd_idx) {
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
        uint32_t* pt = (uint32_t*)pmm_alloc();
        for (int i = 0; i < 1024; i++) pt[i] = 0;
        page_directory[pd_idx] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITE;
    }
    return (uint32_t*)(page_directory[pd_idx] & ~0xFFF);
}

void paging_init() {
    for (int i = 0; i < 1024; i++) page_directory[i] = 0;

    for (uint32_t i = 0; i < 1024; i++)
        page_table_low[i] = (i * 4096) | PAGE_PRESENT | PAGE_WRITE;

    page_directory[0] = (uint32_t)page_table_low | PAGE_PRESENT | PAGE_WRITE;

    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}

void paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;
    uint32_t* pt    = get_or_create_pt(pd_idx);
    pt[pt_idx]      = (phys & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void paging_unmap(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) return;
    uint32_t* pt = (uint32_t*)(page_directory[pd_idx] & ~0xFFF);
    pt[pt_idx] = 0;
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void paging_map_region(uint32_t virt, uint32_t phys,
                       uint32_t size, uint32_t flags) {
    uint32_t pages = (size + 4095) / 4096;
    for (uint32_t i = 0; i < pages; i++)
        paging_map(virt + i*4096, phys + i*4096, flags);
}

bool paging_alloc_region(uint32_t virt, uint32_t size, uint32_t flags) {
    uint32_t pages = (size + 4095) / 4096;
    for (uint32_t i = 0; i < pages; i++) {
        void* phys = pmm_alloc();
        if (!phys) return false; // OOM
        paging_map(virt + i*4096, (uint32_t)phys, flags);
    }
    return true;
}

void* paging_get_physaddr(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) return nullptr;
    uint32_t* pt = (uint32_t*)(page_directory[pd_idx] & ~0xFFF);
    if (!(pt[pt_idx] & PAGE_PRESENT)) return nullptr;
    return (void*)((pt[pt_idx] & ~0xFFF) + (virt & 0xFFF));
}

bool paging_is_mapped(uint32_t virt) {
    return paging_get_physaddr(virt) != nullptr;
}