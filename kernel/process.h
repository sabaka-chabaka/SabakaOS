#pragma once
#include <stdint.h>

#define PROC_MAX        16
#define PROC_STACK_SIZE 8192
#define PROC_NAME_LEN   32

/* Size of the Ring-3 user stack allocated per process. */
#define PROC_USER_STACK_SIZE 8192

/*
 * Virtual base address where user stacks are mapped.
 * Each user process gets its own page-aligned slot:
 *   process N → USER_STACK_VIRT_BASE + N * PROC_USER_STACK_SIZE
 * This keeps them out of the kernel heap region (0x0800_0000).
 */
#define USER_STACK_VIRT_BASE 0x10000000u

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

    bool           is_user;
    uint32_t       user_entry;
    uint32_t       user_stack_virt;

    uint32_t brk_start;
    uint32_t brk_curr;
};

typedef void (*ProcessFunc)(void* arg);
