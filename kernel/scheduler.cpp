#include "scheduler.h"
#include "process.h"
#include "tss.h"
#include "heap.h"
#include "kstring.h"
#include "pit.h"

extern "C" void context_switch(uint32_t* old_esp, uint32_t new_esp);

static Process  procs[PROC_MAX];
static int      proc_count   = 0;
static int      current_idx  = 0;
static bool     initialized  = false;
static uint32_t time_slice   = 10;

static void proc_entry(Process* p, ProcessFunc func, void* arg) {
    __asm__ volatile("sti");
    func(arg);
    process_exit();
}

static Process* alloc_proc() {
    for (int i = 0; i < PROC_MAX; i++)
        if (procs[i].state == PROC_DEAD) return &procs[i];
    return nullptr;
}

void scheduler_init() {
    for (int i = 0; i < PROC_MAX; i++)
        procs[i].state = PROC_DEAD;
    proc_count  = 0;
    current_idx = 0;
    initialized = true;

    Process* idle     = &procs[0];
    idle->pid         = 0;
    kstrcpy(idle->name, "kernel");
    idle->state       = PROC_RUNNING;
    idle->priority    = 1;
    idle->ticks_total = 0;
    idle->ticks_slice = 0;
    idle->stack_base  = 0;
    idle->esp         = 0;
    proc_count        = 1;
}

Process* process_create(ProcessFunc func, void* arg,
                        const char* name, uint32_t priority) {
    Process* p = alloc_proc();
    if (!p) return nullptr;

    static uint32_t next_pid = 1;
    p->pid         = next_pid++;
    p->state       = PROC_READY;
    p->priority    = priority;
    p->ticks_total = 0;
    p->ticks_slice = 0;
    p->sleep_until = 0;
    kstrncpy(p->name, name, PROC_NAME_LEN-1);
    p->name[PROC_NAME_LEN-1] = 0;

    uint8_t* stack = (uint8_t*)kmalloc(PROC_STACK_SIZE);
    if (!stack) { p->state = PROC_DEAD; return nullptr; }
    p->stack_base = (uint32_t)stack;

    uint32_t* sp = (uint32_t*)(stack + PROC_STACK_SIZE);

    *--sp = (uint32_t)arg;
    *--sp = (uint32_t)func;
    *--sp = (uint32_t)p;
    *--sp = 0;

    *--sp = (uint32_t)proc_entry;

    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    p->esp = (uint32_t)sp;

    proc_count++;
    return p;
}

void process_exit() {
    __asm__ volatile("cli");
    procs[current_idx].state = PROC_DEAD;
    proc_count--;
    scheduler_yield();
    // Interrupts were re-enabled by scheduler_yield(); disable them now so
    // the dead process cannot be entered again via the scheduler path.
    for(;;) __asm__ volatile("cli; hlt");
}

void process_block(Process* p) {
    if (p) p->state = PROC_BLOCKED;
}

void process_unblock(Process* p) {
    if (p && p->state == PROC_BLOCKED) p->state = PROC_READY;
}

void process_sleep(uint32_t ms) {
    __asm__ volatile("cli");
    procs[current_idx].state       = PROC_SLEEP;
    // Convert ms to PIT ticks so sleep_until is in the same unit as pit_ticks().
    // pit_uptime_ms() uses pit_frequency, so we mirror that conversion here.
    procs[current_idx].sleep_until = pit_ticks() + (ms * pit_get_frequency()) / 1000;
    scheduler_yield();
}

Process* scheduler_current() {
    return &procs[current_idx];
}

Process* scheduler_get(int idx) {
    if (idx < 0 || idx >= PROC_MAX) return nullptr;
    return &procs[idx];
}

int scheduler_count() { return proc_count; }

static void do_switch(int next_idx) {
    if (next_idx == current_idx) return;

    int prev_idx = current_idx;
    current_idx  = next_idx;

    Process* prev = &procs[prev_idx];
    Process* next = &procs[next_idx];

    if (prev->state == PROC_RUNNING) prev->state = PROC_READY;
    next->state       = PROC_RUNNING;
    next->ticks_slice = 0;

    tss_set_kernel_stack(next->stack_base + PROC_STACK_SIZE);

    context_switch(&prev->esp, next->esp);
}

static int find_next() {
    uint32_t now = pit_ticks();

    for (int i = 0; i < PROC_MAX; i++) {
        if (procs[i].state == PROC_SLEEP && now >= procs[i].sleep_until)
            procs[i].state = PROC_READY;
    }

    for (int i = 1; i <= PROC_MAX; i++) {
        int idx = (current_idx + i) % PROC_MAX;
        if (procs[idx].state == PROC_READY) return idx;
    }

    // Fall back to the idle/kernel process only if it is actually runnable.
    if (procs[0].state == PROC_READY || procs[0].state == PROC_RUNNING) return 0;
    // No runnable process found — stay on current (busy-wait in idle hlt loop).
    return current_idx;
}

void scheduler_tick() {
    if (!initialized) return;

    procs[current_idx].ticks_total++;
    procs[current_idx].ticks_slice++;

    if (procs[current_idx].ticks_slice >= time_slice ||
        procs[current_idx].state == PROC_DEAD  ||
        procs[current_idx].state == PROC_SLEEP ||
        procs[current_idx].state == PROC_BLOCKED) {

        int next = find_next();
        if (next != current_idx) do_switch(next);
    }
}

void scheduler_yield() {
    __asm__ volatile("cli");
    int next = find_next();
    if (next != current_idx) do_switch(next);
    __asm__ volatile("sti");
}