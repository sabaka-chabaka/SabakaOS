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

#define USER_CODE_VIRT 0x20000000u
#define USER_CODE_SIZE 4096u

__attribute__((section(".user_text"), noinline))
static void user_hello_proc() {
    __asm__ volatile(
        "jmp skip_data\n"
        "msg_ok:  .ascii \"[BRK TEST] Memory allocated and writable!\\n\"\n"
        "msg_err: .ascii \"[BRK TEST] Failed to allocate memory...\\n\"\n"
        "skip_data:\n"

        "mov $45, %%eax\n"
        "xor %%ebx, %%ebx\n"
        "int $0x80\n"
        "mov %%eax, %%esi\n"

        "add $4096, %%eax\n"
        "mov %%eax, %%ebx\n"
        "mov $45, %%eax\n"
        "int $0x80\n"

        "cmp %%esi, %%eax\n"
        "je fail\n"

        "movb $'!', (%%esi)\n"

        "mov $4, %%eax\n"
        "mov $1, %%ebx\n"
        "lea msg_ok, %%ecx\n"
        "mov $38, %%edx\n"
        "int $0x80\n"
        "jmp exit\n"

    "fail:\n"
        "mov $4, %%eax\n"
        "mov $1, %%ebx\n"
        "lea msg_err, %%ecx\n"
        "mov $35, %%edx\n"
        "int $0x80\n"

    "exit:\n"
        "mov $1, %%eax\n"
        "xor %%ebx, %%ebx\n"
        "int $0x80\n"
        ::: "eax", "ebx", "ecx", "edx", "esi"
    );
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

extern "C" void kernel_main() {
    for(int i=0;i<80*25;i++) VGA[i]=ve(' ',15,0);

    vga_fill(' ', 0, 15, 1);
    vga_print("  SabakaOS v0.2.0", 0, 0, 15, 1);
    vga_print("[x86 | Ring3 | Syscall | Mutex | Pipe]", 0, 33, 14, 1);

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

    /* ---- Spawn Ring-0 kernel processes ---- */
    process_create(mutex_proc_a, nullptr, "mutex_a",   5);
    process_create(mutex_proc_b, nullptr, "mutex_b",   5);

    int pipe_id = pipe_create();
    if (pipe_id >= 0) {
        process_create(pipe_producer, (void*)(uintptr_t)pipe_id, "pipe_prod", 5);
        process_create(pipe_consumer, (void*)(uintptr_t)pipe_id, "pipe_cons", 5);
    }

    /* ---- Spawn Ring-3 usermode process ----
     *
     * 1. Allocate a PAGE_USER page at USER_CODE_VIRT.
     * 2. Copy the user_hello_proc() machine code there.
     * 3. Create the process with process_create_user().
     *
     * Copying the code is necessary because the original function lives in
     * the kernel's .text section which is NOT mapped PAGE_USER — attempting
     * to execute it at CPL=3 would cause a #PF (protection fault).
     */
    if (paging_alloc_region(USER_CODE_VIRT, USER_CODE_SIZE,
                            PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) {
        /* Copy function bytes to user page.
           We assume user_hello_proc() fits in USER_CODE_SIZE (4 KB) which
           is safe for such a tiny function. */
        uint8_t* dst = (uint8_t*)USER_CODE_VIRT;
        uint8_t* src = (uint8_t*)(uintptr_t)user_hello_proc;
        for (uint32_t i = 0; i < USER_CODE_SIZE; i++)
            dst[i] = src[i];

        process_create_user(USER_CODE_VIRT, "user_hello", 5);
    }

    /* ---- Terminal / shell ---- */
    vga_fill('-', 1, 8);

    terminal_init();
    shell_init();
    terminal_set_execute_cb(shell_execute);
    keyboard_set_callback(terminal_on_key);

    terminal_set_color_fg(11);
    terminal_puts("SabakaOS v0.2.0\n");
    terminal_reset_color();
    terminal_puts("Ring-3 usermode (CPL=3) + syscall(int 0x80) + mutex + pipe\n");
    terminal_puts("type 'ps' to watch processes, 'help' for commands\n\n");

    terminal_reply_input();

    __asm__ volatile("sti");
    for(;;) __asm__ volatile("hlt");
}