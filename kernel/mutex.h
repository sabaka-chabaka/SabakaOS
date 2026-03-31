#pragma once
#include <stdint.h>

struct Mutex {
    volatile int locked;
    uint32_t      owner;
};

struct Semaphore {
    volatile int count;
    uint32_t      max;
};

void mutex_init(Mutex* m);
void mutex_lock(Mutex* m);
void mutex_unlock(Mutex* m);
bool mutex_trylock(Mutex* m);

void sem_init(Semaphore* s, int initial, uint32_t max);
void sem_wait(Semaphore* s);
void sem_signal(Semaphore* s);
int sem_value(Semaphore* s);