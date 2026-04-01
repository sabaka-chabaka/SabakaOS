#include "pipe.h"
#include "kstring.h"
#include "scheduler.h"

static Pipe pipes[PIPE_MAX];

void pipe_init_all() {
    for (int i = 0; i < PIPE_MAX; i++) {
        pipes[i].used         = false;
        pipes[i].read_pos     = 0;
        pipes[i].write_pos    = 0;
        pipes[i].count        = 0;
        pipes[i].write_closed = false;
    }
}

int pipe_create() {
    for (int i = 0; i < PIPE_MAX; i++) {
        if (!pipes[i].used) {
            pipes[i].used         = true;
            pipes[i].read_pos     = 0;
            pipes[i].write_pos    = 0;
            pipes[i].count        = 0;
            pipes[i].write_closed = false;
            return i;
        }
    }
    return -1;
}

void pipe_destroy(int id) {
    if (id < 0 || id >= PIPE_MAX) return;
    pipes[id].used = false;
}

void pipe_close_write(int id) {
    if (id >= 0 && id < PIPE_MAX && pipes[id].used) pipes[id].write_closed = true;
}

bool pipe_empty(int id) {
    if (id < 0 || id >= PIPE_MAX || !pipes[id].used) return true;
    return pipes[id].count == 0;
}

int pipe_write(int id, const char* buf, uint32_t len) {
    if (id < 0 || id >= PIPE_MAX || !pipes[id].used) return -1;
    Pipe& p = pipes[id];

    uint32_t written = 0;
    while (written < len) {
        while (p.count >= PIPE_BUF_SIZE)
            scheduler_yield();

        __asm__ volatile("cli");
        uint32_t space = PIPE_BUF_SIZE - p.count;
        uint32_t chunk = len - written;
        if (chunk > space) chunk = space;

        for (uint32_t i = 0; i < chunk; i++) {
            p.buf[p.write_pos] = buf[written + i];
            p.write_pos = (p.write_pos + 1) % PIPE_BUF_SIZE;
        }
        p.count  += chunk;
        written  += chunk;
        __asm__ volatile("sti");
    }
    return (int)written;
}

int pipe_read(int id, char* buf, uint32_t len) {
    if (id < 0 || id >= PIPE_MAX || !pipes[id].used) return -1;
    Pipe& p = pipes[id];

    while (p.count == 0 && !p.write_closed)
        scheduler_yield();

    if (p.count == 0) return 0;

    __asm__ volatile("cli");
    uint32_t avail = p.count < len ? p.count : len;
    for (uint32_t i = 0; i < avail; i++) {
        buf[i] = p.buf[p.read_pos];
        p.read_pos = (p.read_pos + 1) % PIPE_BUF_SIZE;
    }
    p.count -= avail;
    __asm__ volatile("sti");
    return (int)avail;
}

int pipe_read_nb(int id, char* buf, uint32_t len) {
    if (id < 0 || id >= PIPE_MAX || !pipes[id].used) return -1;
    Pipe& p = pipes[id];
    if (p.count == 0) return 0;

    __asm__ volatile("cli");
    uint32_t avail = p.count < len ? p.count : len;
    for (uint32_t i = 0; i < avail; i++) {
        buf[i] = p.buf[p.read_pos];
        p.read_pos = (p.read_pos + 1) % PIPE_BUF_SIZE;
    }
    p.count -= avail;
    __asm__ volatile("sti");
    return (int)avail;
}