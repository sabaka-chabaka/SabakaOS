#include "pit.h"

static const uint32_t PIT_BASE_FREQ = 1193182;
static const uint16_t PIT_COMMAND   = 0x43;
static const uint16_t PIT_CHANNEL0  = 0x40;

static volatile uint32_t pit_tick_count = 0;
static uint32_t pit_frequency = 1000;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1" :: "a"(val), "Nd"(port));
}

void pit_init(uint32_t frequency) {
    pit_frequency = frequency;
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

void pit_tick() {
    pit_tick_count++;
}

uint32_t pit_ticks() {
    return pit_tick_count;
}

uint32_t pit_uptime_ms() {
    return pit_tick_count * 1000 / pit_frequency;
}

uint32_t pit_uptime_sec() {
    return pit_tick_count / pit_frequency;
}

void pit_sleep_ms(uint32_t ms) {
    uint32_t ticks = ms * pit_frequency / 1000;
    uint32_t start = pit_tick_count;
    while (pit_tick_count - start < ticks)
        __asm__ volatile("hlt");
}

void pit_sleep(uint32_t seconds) {
    pit_sleep_ms(seconds * 1000);
}
