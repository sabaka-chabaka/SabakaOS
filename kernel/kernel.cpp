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
#include "fb.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int VGA_COLS = 80;
static inline unsigned short ve(char c, unsigned char fg, unsigned char bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}
static void vga_print(const char* s, int row, int col, unsigned char fg=15, unsigned char bg=0) {
    for(int i=0;s[i]&&col+i<VGA_COLS;i++) VGA[row*VGA_COLS+col+i]=ve(s[i],fg,bg);
}
static void vga_fill_row(int row, unsigned char fg, unsigned char bg=0) {
    for(int i=0;i<VGA_COLS;i++) VGA[row*VGA_COLS+i]=ve(' ',fg,bg);
}

#define HEAP_VIRT (8u * 1024u * 1024u)
#define HEAP_SIZE (4u * 1024u * 1024u)

static Mutex shared_mutex;
static volatile uint32_t shared_counter = 0;

static void mutex_proc_a(void*) {
    for(int i=0;i<5;i++){mutex_lock(&shared_mutex);shared_counter++;mutex_unlock(&shared_mutex);process_sleep(200);}
    process_exit();
}

static void mutex_proc_b(void*) {
    for(int i=0;i<5;i++){mutex_lock(&shared_mutex);shared_counter++;mutex_unlock(&shared_mutex);process_sleep(300);}
    process_exit();
}

static void draw_header_fb() {
    uint32_t W = fb_width();
    fb_fill_rect(0, 0, (int)W, 28, Color::HeaderBg);
    fb_draw_hline(0, 27, (int)W, Color::PromptArrow);

    fb_draw_str(8, 6, "SabakaOS", Color::PromptArrow, Color::HeaderBg);
    fb_draw_str(8 + 8*8 + 4, 6, "v0.4.3", Color::White, Color::HeaderBg);

    uint32_t H = fb_height();
    char ws[12], hs[12], bpps[4];
    kuitoa(W, ws, 10); kuitoa(H, hs, 10); kuitoa(fb_bpp(), bpps, 10);
    char res[32]; int ri = 0;
    for(int i=0;ws[i];i++) res[ri++]=ws[i];
    res[ri++]='x';
    for(int i=0;hs[i];i++) res[ri++]=hs[i];
    res[ri++]=' ';
    for(int i=0;bpps[i];i++) res[ri++]=bpps[i];
    res[ri++]='b'; res[ri++]='p'; res[ri++]='p'; res[ri]=0;

    int rx = (int)W - ri * FB_FONT_W - 8;
    fb_draw_str(rx, 6, res, Color::Cyan, Color::HeaderBg);

    fb_fill_rect(0, 28, (int)W, (int)H - 28, Color::Black);
}

extern "C" void kernel_main(uint32_t mb_magic, MultibootInfo* mb_info) {

    bool have_fb = (mb_magic == 0x2BADB002) && fb_init(mb_info);

    if (have_fb) {
        draw_header_fb();
    } else {
        for(int i=0;i<80*25;i++) VGA[i]=ve(' ',15,0);
        vga_fill_row(0, 15, 1);
        vga_print("  SabakaOS v0.4.3 x86  [VGA text — GRUB не дал VESA]", 0, 0, 15, 1);
    }

    gdt_init();
    idt_init();
    pmm_init(32 * 1024 * 1024);
    paging_init();

    bool heap_ok = paging_alloc_region(HEAP_VIRT, HEAP_SIZE, PAGE_PRESENT | PAGE_WRITE);
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

    bool t_init = false;
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

    process_create(mutex_proc_a, nullptr, "mutex_a", 5);
    process_create(mutex_proc_b, nullptr, "mutex_b", 5);

    if (!t_init) terminal_init();
    shell_init();
    terminal_set_execute_cb(shell_execute);
    keyboard_set_callback(terminal_on_key);

    terminal_reply_input();

    __asm__ volatile("sti");
    for(;;) __asm__ volatile("hlt");
}