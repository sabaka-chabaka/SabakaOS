#include "gdt.h"

static GDTEntry gdt_entries[3];
static GDTPointer gdt_ptr;

static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t granularity) {
    GDTEntry& e = gdt_entries[index];

    e.base_low    = (base & 0xFFFF);
    e.base_middle = (base >> 16) & 0xFF;
    e.base_high   = (base >> 24) & 0xFF;

    e.limit_low   = (limit & 0xFFFF);
    e.granularity = (limit >> 16) & 0x0F;

    e.granularity |= (granularity & 0xF0);

    e.access = access;
}

void gdt_init() {
    gdt_ptr.limit = (sizeof(GDTEntry) * 3) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    gdt_set_entry(0, 0, 0, 0, 0);

    gdt_set_entry(1,
        0,
        0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
        GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
        GDT_GRAN_4K | GDT_GRAN_32BIT | GDT_GRAN_LIMIT_HIGH
    );

    gdt_set_entry(2,
        0,
        0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
        GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_RW,
        GDT_GRAN_4K | GDT_GRAN_32BIT | GDT_GRAN_LIMIT_HIGH
    );


    gdt_flush((uint32_t)&gdt_ptr);
}