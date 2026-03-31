#include "mutex.h"

#include "process.h"
#include "scheduler.h"

static int atomic_xchg(volatile int* ptr, int val) {
    int old;
    __asm__ volatile(
        "xchgl %0, %1"
        : "=r"(old), "+m"(*ptr)
        : "0"(val)
        : "memory"
    );
    return old;
}

void mutex_init(Mutex* m) {
    m->locked = 0;
    m->owner  = (uint32_t)-1;
}

bool mutex_trylock(Mutex* m) {
    int old = atomic_xchg(&m->locked, 1);
    if (old == 0) {
        Process* cur = scheduler_current();
        m->owner = cur ? cur->pid : 0;
        return true;
    }
    return false;
}

void mutex_lock(Mutex* m) {
    Process* cur = scheduler_current();
    if (cur && m->locked && m->owner == cur->pid) return;

    while (!mutex_trylock(m)) scheduler_yield();
}

void mutex_unlock(Mutex* m) {
    m->owner  = (uint32_t)-1;
    __asm__ volatile("" ::: "memory");
    m->locked = 0;
}

void sem_init(Semaphore* s, int initial, uint32_t max) {
    s->count = initial;
    s->max   = max;
}

void sem_wait(Semaphore* s) {
    while (true) {
        __asm__ volatile("cli");
        if (s->count > 0) {
            s->count--;
            __asm__ volatile("sti");
            return;
        }
        __asm__ volatile("sti");
        scheduler_yield();
    }
}

void sem_signal(Semaphore *s) {
    __asm__ volatile("cli");
    if ((uint32_t)s->count < s->max) s->count++;
    __asm__ volatile("sti");
}

int sem_value(Semaphore *s) {
    return s->count;
}