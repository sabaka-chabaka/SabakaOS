#pragma once
#include <stdint.h>

#define SYS_EXIT      1
#define SYS_FORK      2
#define SYS_READ      3
#define SYS_WRITE     4
#define SYS_OPEN      5
#define SYS_CLOSE     6
#define SYS_GETPID    20
#define SYS_SLEEP     162
#define SYS_MALLOC    192
#define SYS_FREE      91
#define SYS_BRK       45
#define SYS_FSTAT     108
#define SYS_UNAME     122

#define SYS_SABAKA_SPAWN  300
#define SYS_SABAKA_PS     301

void syscall_init();

struct Registers;
extern "C" int32_t syscall_dispatch(Registers* regs);