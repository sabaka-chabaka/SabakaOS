#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "kstring.h"
#include "terminal.h"
#include "shell.h"
#include "pit.h"
#include "vfs.h"
#include "tss.h"
#include "scheduler.h"
#include "syscall.h"
#include "mutex.h"
#include "pipe.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int COLS = 80;

static inline unsigned short ve(char c, unsigned char fg, unsigned char bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}
static void vga_print(const char* s, int row, int col,
                      unsigned char fg=15, unsigned char bg=0) {
    for(int i=0;s[i]&&col+i<COLS;i++) VGA[row*COLS+col+i]=ve(s[i],fg,bg);
}
static void vga_fill(char c, int row, unsigned char fg, unsigned char bg=0) {
    for(int i=0;i<COLS;i++) VGA[row*COLS+i]=ve(c,fg,bg);
}

#define HEAP_VIRT (8u * 1024u * 1024u)
#define HEAP_SIZE (4u * 1024u * 1024u)

static void syscall_test_proc(void*) {
    const char* msg = "[syscall] Hello from process via SYS_WRITE!\n";
    __asm__ volatile(
        "mov $1, %%eax\n"
        "mov $1, %%ebx\n"
        "mov %0, %%ecx\n"
        "mov $44, %%edx\n"
        "int $0x80\n"
        :: "r"(msg)
        : "eax","ebx","ecx","edx"
    );
    process_exit();
}

static Mutex shared_mutex;
static volatile uint32_t shared_counter = 0;

static void mutex_proc_a(void*) {
    for (int i = 0; i < 5; i++) {
        mutex_lock(&shared_mutex);
        shared_counter++;
        mutex_unlock(&shared_mutex);
        process_sleep(200);
    }
    process_exit();
}

static void mutex_proc_b(void*) {
    for (int i = 0; i < 5; i++) {
        mutex_lock(&shared_mutex);
        shared_counter++;
        mutex_unlock(&shared_mutex);
        process_sleep(300);
    }
    process_exit();
}

static void pipe_producer(void* arg) {
    int pipe_id = (int)(uintptr_t)arg;
    const char* msgs[] = { "hello", "from", "pipe", nullptr };
    for (int i = 0; msgs[i]; i++) {
        pipe_write(pipe_id, msgs[i], kstrlen(msgs[i]));
        pipe_write(pipe_id, "\n", 1);
        process_sleep(100);
    }
    pipe_close_write(pipe_id);
    process_exit();
}

static void pipe_consumer(void* arg) {
    int pipe_id = (int)(uintptr_t)arg;
    char buf[64];
    int n;
    while ((n = pipe_read(pipe_id, buf, 63)) > 0) {
        buf[n] = 0;
        for (int i = 0; i < n; i++)
            terminal_putchar(buf[i]);
    }
    pipe_destroy(pipe_id);
    process_exit();
}

static void idle_proc(void*) {
    while (true) __asm__ volatile("hlt");
}

extern "C" void kernel_main() {
    for(int i=0;i<80*25;i++) VGA[i]=ve(' ',15,0);

    vga_fill(' ',0,15,1);
    vga_print("  SabakaOS v0.2.0",0,0,15,1);
    vga_print("[x86 | Protected Mode | Syscall | Mutex | Pipe]",0,32,14,1);

    gdt_init();
    idt_init();
    pmm_init(32 * 1024 * 1024);
    paging_init();

    bool heap_ok = paging_alloc_region(HEAP_VIRT, HEAP_SIZE,
                                       PAGE_PRESENT | PAGE_WRITE);
    if (heap_ok) heap_init(HEAP_VIRT, HEAP_SIZE);

    tss_init();
    pit_init(1000);
    vfs_init();
    keyboard_init();
    syscall_init();
    pipe_init_all();
    mutex_init(&shared_mutex);

    scheduler_init();
    process_create(idle_proc, nullptr, "idle", 1);

    process_create(syscall_test_proc, nullptr,  "syscall_test", 5);
    process_create(mutex_proc_a,      nullptr,  "mutex_a",      5);
    process_create(mutex_proc_b,      nullptr,  "mutex_b",      5);

    int pipe_id = pipe_create();
    if (pipe_id >= 0) {
        process_create(pipe_producer, (void*)(uintptr_t)pipe_id, "pipe_prod", 5);
        process_create(pipe_consumer, (void*)(uintptr_t)pipe_id, "pipe_cons", 5);
    }

    vga_fill('-', 1, 8);
    vga_fill(' ', 24, 15, 1);
    vga_print("  SabakaOS v0.2.0 | syscall+mutex+pipe | type 'help'", 24, 0, 15, 1);

    terminal_init();
    shell_init();
    terminal_set_execute_cb(shell_execute);
    keyboard_set_callback(terminal_on_key);

    terminal_set_color_fg(11);
    terminal_puts("SabakaOS v0.2.0\n");
    terminal_reset_color();
    terminal_puts("syscall(int 0x80), mutex, pipe — ready\n");
    terminal_puts("type 'ps' to watch processes, 'help' for commands\n\n");

    terminal_reply_input();

    for(;;) __asm__ volatile("hlt");
}