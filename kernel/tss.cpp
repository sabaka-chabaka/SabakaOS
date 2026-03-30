#include "tss.h"
#include "gdt.h"

static TSS tss;
extern void gdt_set_tss_entry(uint32_t base, uint32_t limit);

extern "C" void tss_flush();

void tss_init() {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(TSS) - 1;

    for (uint32_t i = 0; i < sizeof(TSS); i++)
        ((uint8_t*)&tss)[i] = 0;

    tss.ss0 = 0x10;
    tss.esp0 = 0;
    tss.iomap_base = sizeof(TSS);

    gdt_set_tss_entry(base, limit);
    tss_flush();
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}