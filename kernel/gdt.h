#pragma once
#include <stdint.h>

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct GDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define GDT_ACCESS_PRESENT    0x80
#define GDT_ACCESS_RING0      0x00
#define GDT_ACCESS_RING3      0x60
#define GDT_ACCESS_DESCRIPTOR 0x10
#define GDT_ACCESS_EXECUTABLE 0x08
#define GDT_ACCESS_RW         0x02
#define GDT_ACCESS_TSS        0x09

#define GDT_GRAN_4K           0x80
#define GDT_GRAN_32BIT        0x40
#define GDT_GRAN_LIMIT_HIGH   0x0F

#define GDT_NULL_SEGMENT   0x00
#define GDT_KERNEL_CODE    0x08
#define GDT_KERNEL_DATA    0x10
#define GDT_TSS_SEGMENT    0x18

void gdt_init();
void gdt_set_tss_entry(uint32_t base, uint32_t limit);

extern "C" void gdt_flush(uint32_t gdt_pointer);