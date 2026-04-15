#include "ata.h"
#include "fat32.h"
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
#include "net.h"
#include "pipe.h"
#include "rtl8139.h"
#include "vfs_disk.h"

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

extern "C" void kernel_main() {
    for(int i=0;i<80*25;i++) VGA[i]=ve(' ',15,0);

    vga_fill(' ', 0, 15, 1);
    vga_print("  SabakaOS v0.4.3 x86", 0, 0, 15, 1);

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

    if (rtl8139_init()) {
        net_init(
            ip_from_str("10.0.2.15"),
            ip_from_str("10.0.2.2"),
            ip_from_str("255.255.255.0")
        );
    }

    bool t_init;

    if (ata_init()) {
        t_init = true;
        terminal_init();
        terminal_set_color_fg(10);
        terminal_puts("[ATA] Disk found, ");
        char sec_buf[16]; kuitoa(ata_sectors_count(), sec_buf, 10);
        terminal_puts(sec_buf);
        terminal_puts(" sectors\n");
        terminal_reset_color();

        if (fat32_init()) {
            if (vfs_mount_fat32("/disk")) {
                terminal_set_color_fg(10);
                terminal_puts("[FAT32] Mounted at /disk\n");
                terminal_reset_color();
            } else {
                terminal_set_color_fg(14);
                terminal_puts("[FAT32] VFS mount failed\n");
                terminal_reset_color();
            }
        } else {
            terminal_set_color_fg(14);
            terminal_puts("[ATA] FAT32 not found on disk\n");
            terminal_reset_color();
        }
    } else {
        terminal_set_color_fg(14);
        terminal_puts("[ATA] No disk detected\n");
        terminal_reset_color();
    }

    keyboard_init();
    syscall_init();
    pipe_init_all();
    mutex_init(&shared_mutex);

    scheduler_init();

    process_create(mutex_proc_a, nullptr, "mutex_a",   5);
    process_create(mutex_proc_b, nullptr, "mutex_b",   5);

    if (!t_init) terminal_init();
    shell_init();
    terminal_set_execute_cb(shell_execute);
    keyboard_set_callback(terminal_on_key);

    terminal_reply_input();

    __asm__ volatile("sti");
    for(;;) __asm__ volatile("hlt");
}