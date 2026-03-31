#pragma once
#include <stdint.h>

#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_OPEN 3
#define SYS_CLOSE 4
#define SYS_GETPID 5
#define SYS_SLEEP 6
#define SYS_MALLOC 7
#define SYS_FREE 8
#define SYS_SPAWN 9

#define SYSCALL_COUNT 10

struct SyscallArgs {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

void syscall_init();