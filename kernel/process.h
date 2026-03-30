#pragma once
#include <stdint.h>

#define PROC_MAX 16
#define PROC_STACK_SIZE 8192
#define PROC_NAME_LEN 32

enum ProcessState {
    PROC_DEAD    = 0,
    PROC_READY   = 1,
    PROC_RUNNING = 2,
    PROC_BLOCKED = 3,
    PROC_SLEEP   = 4,
};

struct ProcessContext {
    uint32_t edi, esi, ebp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t eip;
    uint32_t eflags;
};

struct Process {
    uint32_t       pid;
    char           name[PROC_NAME_LEN];
    ProcessState   state;
    uint32_t       esp;
    uint32_t       stack_base;
    uint32_t       sleep_until;
    uint32_t       priority;
    uint32_t       ticks_total;
    uint32_t       ticks_slice;
};

typedef void (*ProcessFunc)(void* arg);