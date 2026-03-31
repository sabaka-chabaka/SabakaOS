#include "syscall.h"
#include "terminal.h"
#include "heap.h"
#include "scheduler.h"
#include "vfs.h"
#include "kstring.h"
#include "idt.h"

#define MAX_FD 16

struct FDEntry {
    bool     used;
    VfsNode* node;
    uint32_t offset;
};

static FDEntry fd_table[MAX_FD];

static int alloc_fd(VfsNode* node) {
    for (int i = 3; i < MAX_FD; i++) {
        if (!fd_table[i].used) {
            fd_table[i] = { true, node, 0 };
            return i;
        }
    }
    return -1;
}

static int32_t sys_exit(uint32_t code) {
    (void)code;
    process_exit();
    return 0;
}

static int32_t sys_write(uint32_t fd, uint32_t buf_addr, uint32_t len) {
    if (len > 4096) len = 4096;
    const char* buf = (const char*)buf_addr;

    if (fd == 1 || fd == 2) {
        for (uint32_t i = 0; i < len; i++)
            terminal_putchar(buf[i]);
        return (int32_t)len;
    }

    if (fd < MAX_FD && fd_table[fd].used) {
        int r = vfs_write(fd_table[fd].node,
                          (const uint8_t*)buf,
                          fd_table[fd].offset, len);
        if (r > 0) fd_table[fd].offset += r;
        return r;
    }
    return -1;
}

static int32_t sys_read(uint32_t fd, uint32_t buf_addr, uint32_t len) {
    if (fd == 0) {
        return -1;
    }
    if (fd < MAX_FD && fd_table[fd].used) {
        int r = vfs_read(fd_table[fd].node,
                         (uint8_t*)buf_addr,
                         fd_table[fd].offset, len);
        if (r > 0) fd_table[fd].offset += r;
        return r;
    }
    return -1;
}

static int32_t sys_open(uint32_t path_addr, uint32_t /*flags*/, uint32_t /*mode*/) {
    const char* path = (const char*)path_addr;
    VfsNode* node = vfs_resolve_path(path);
    if (!node) return -1;
    return alloc_fd(node);
}

static int32_t sys_close(uint32_t fd) {
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].used) return -1;
    fd_table[fd].used = false;
    return 0;
}

static int32_t sys_getpid() {
    return (int32_t)scheduler_current()->pid;
}

static int32_t sys_sleep(uint32_t ms) {
    process_sleep(ms);
    return 0;
}

static int32_t sys_malloc(uint32_t size) {
    return (int32_t)(uintptr_t)kmalloc(size);
}

static int32_t sys_free(uint32_t ptr) {
    kfree((void*)(uintptr_t)ptr);
    return 0;
}

typedef int32_t (*SyscallFn)();

extern "C" int32_t syscall_dispatch(Registers* regs) {
    uint32_t num = regs->eax;
    uint32_t a   = regs->ebx;
    uint32_t b   = regs->ecx;
    uint32_t c   = regs->edx;

    switch (num) {
        case SYS_EXIT:   return sys_exit(a);
        case SYS_WRITE:  return sys_write(a, b, c);
        case SYS_READ:   return sys_read(a, b, c);
        case SYS_OPEN:   return sys_open(a, b, c);
        case SYS_CLOSE:  return sys_close(a);
        case SYS_GETPID: return sys_getpid();
        case SYS_SLEEP:  return sys_sleep(a);
        case SYS_MALLOC: return sys_malloc(a);
        case SYS_FREE:   return sys_free(a);
        default:
            return -1;
    }
}

void syscall_init() {
    for (int i = 0; i < MAX_FD; i++)
        fd_table[i].used = false;
}