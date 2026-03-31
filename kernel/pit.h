#pragma once
#include <stdint.h>

void pit_init(uint32_t frequency);
void pit_tick();
uint32_t pit_ticks();
uint32_t pit_get_frequency();
uint32_t pit_uptime_ms();
uint32_t pit_uptime_sec();
void pit_sleep_ms(uint32_t ms);
void pit_sleep(uint32_t seconds);
