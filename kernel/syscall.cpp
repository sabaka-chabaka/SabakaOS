#include "syscall.h"
#include "terminal.h"
#include "heap.h"
#include "scheduler.h"
#include "vfs.h"
#include "kstring.h"
#include "idt.h"
#include "pit.h"

#define MAX_FD 32

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
    if (len > 65536) len = 65536;
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
    if (fd == 0) return -1; // stdin TODO
    if (fd < MAX_FD && fd_table[fd].used) {
        int r = vfs_read(fd_table[fd].node,
                         (uint8_t*)buf_addr,
                         fd_table[fd].offset, len);
        if (r > 0) fd_table[fd].offset += r;
        return r;
    }
    return -1;
}

static int32_t sys_open(uint32_t path_addr, uint32_t flags, uint32_t /*mode*/) {
    const char* path = (const char*)path_addr;
    VfsNode* node = vfs_resolve_path(path);

    if (!node && (flags & 0x40)) {
        const char* slash = path;
        const char* last = path;
        for (const char* p = path; *p; p++)
            if (*p == '/') last = p + 1;
        node = vfs_create(vfs_cwd(), last);
    }
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

static int32_t sys_nanosleep(uint32_t timespec_addr) {
    if (!timespec_addr) return -1;
    uint32_t* ts = (uint32_t*)timespec_addr;
    uint32_t ms = ts[0] * 1000 + ts[1] / 1000000;
    if (ms > 0) process_sleep(ms);
    return 0;
}

static uint32_t current_brk = 0;
static int32_t sys_brk(uint32_t addr) {
    if (addr == 0) return (int32_t)current_brk;
    current_brk = addr;
    return (int32_t)current_brk;
}

static int32_t sys_mmap2(uint32_t /*addr*/, uint32_t len,
                          uint32_t /*prot*/, uint32_t /*flags*/,
                          uint32_t /*fd*/, uint32_t /*pgoff*/) {
    void* p = kmalloc(len);
    return p ? (int32_t)(uintptr_t)p : -1;
}

static int32_t sys_munmap(uint32_t addr, uint32_t /*len*/) {
    kfree((void*)(uintptr_t)addr);
    return 0;
}

struct UtsName {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

static int32_t sys_uname(uint32_t buf_addr) {
    if (!buf_addr) return -1;
    UtsName* u = (UtsName*)buf_addr;
    kstrcpy(u->sysname,   "SabakaOS");
    kstrcpy(u->nodename,  "sabaka");
    kstrcpy(u->release,   "0.2.0");
    kstrcpy(u->version,   "#1 SMP");
    kstrcpy(u->machine,   "i686");
    return 0;
}

extern "C" int32_t syscall_dispatch(Registers* regs) {
    uint32_t num = regs->eax;
    uint32_t a   = regs->ebx;
    uint32_t b   = regs->ecx;
    uint32_t c   = regs->edx;

    switch (num) {
        case SYS_EXIT:   return sys_exit(a);
        case SYS_READ:   return sys_read(a, b, c);
        case SYS_WRITE:  return sys_write(a, b, c);
        case SYS_OPEN:   return sys_open(a, b, c);
        case SYS_CLOSE:  return sys_close(a);
        case SYS_GETPID: return sys_getpid();
        case SYS_SLEEP:  return sys_nanosleep(a);
        case SYS_BRK:    return sys_brk(a);
        case SYS_MALLOC: return sys_mmap2(a, b, c, regs->esi, regs->edi, 0);
        case SYS_FREE:   return sys_munmap(a, b);
        case SYS_UNAME:  return sys_uname(a);
        default:         return -38;
    }
}

void syscall_init() {
    for (int i = 0; i < MAX_FD; i++)
        fd_table[i].used = false;
    current_brk = 0x06000000;
}