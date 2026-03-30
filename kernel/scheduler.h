#pragma once
#include "process.h"

void     scheduler_init();

Process* process_create(ProcessFunc func, void* arg, const char* name,
                        uint32_t priority = 5);

void     process_exit();

void     process_block(Process* p);
void     process_unblock(Process* p);

void     process_sleep(uint32_t ms);

void     scheduler_tick();

void     scheduler_yield();

Process* scheduler_current();

Process* scheduler_get(int idx);
int      scheduler_count();