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
#include "graphics.h"
#include "mouse.h"
#include "usb/uhci.h"
#include "usb/hid.h"
#include "../graphics/font.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int VGA_COLS = 80;
static inline unsigned short ve(char c, unsigned char fg, unsigned char bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}
static void vga_cls() { for(int i=0;i<80*25;i++) VGA[i]=ve(' ',15,0); }
static void vga_fill_row(int row, unsigned char bg) {
    for(int i=0;i<VGA_COLS;i++) VGA[row*VGA_COLS+i]=ve(' ',15,bg);
}
static void vga_print(const char* s, int row, int col,
                      unsigned char fg=15, unsigned char bg=0) {
    for(int i=0;s[i]&&col+i<VGA_COLS;i++)
        VGA[row*VGA_COLS+col+i]=ve(s[i],fg,bg);
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

static void gfx_test() {
    Painter& dp = gfx_painter();
    dp.fill_rect_gradient_v(
        Rect(0, 0, gfx_width(), gfx_height()),
        0xFF0D1117, 0xFF161B22);

    dp.color = color_rgba(255,255,255, 6);
    for (int x = 0; x < gfx_width();  x += 32) dp.draw_vline(x, 0, gfx_height());
    for (int y = 0; y < gfx_height(); y += 32) dp.draw_hline(0, y, gfx_width());

    int w1 = win_create(60, 50, 400, 300, "Welcome to SabakaOS");
    {
        Painter& p = win_painter(w1);
        Rect cr(0, 0, 400 - BORDER_W*2, 300 - TITLEBAR_H - BORDER_W);

        p.fill_rect_gradient_v(cr, 0xFF1E1E2E, 0xFF12121C);

        p.transparent_bg = true;
        p.color = 0xFFBD93F9;
        p.draw_text(20, 16, "SabakaOS");
        p.color = 0xFF44475A;
        p.draw_hline(20, 16 + FONT_H + 3, font_str_width("SabakaOS"));

        struct { Color32 c; const char* s; } info[] = {
            { 0xFF6C7086, "Version  : v0.4.3 x86"         },
            { 0xFF6C7086, "Display  : VESA 1024x768 32bpp" },
            { 0xFF6C7086, "Driver   : Bochs VBE (BGA)"     },
        };
        int iy = 52;
        for (auto& ln : info) {
            p.color = ln.c; p.draw_text(20, iy, ln.s); iy += FONT_H + 2;
        }

        p.color = 0xFF2D2D3F;
        p.draw_hline(20, iy + 4, cr.w - 40);
        iy += 14;

        struct { bool ok; const char* s; } status[] = {
            { true,  "GDT / IDT / PIT"    },
            { true,  "Paging / Heap"      },
            { true,  "Scheduler"          },
            { true,  "VESA Framebuffer"   },
            { true,  "Graphics API"       },
        };
        for (auto& st : status) {
            Color32 dot = st.ok ? 0xFF50FA7B : 0xFFFF5555;
            p.color = dot;
            p.fill_circle(30, iy + FONT_H/2, 4);
            p.color = 0xFFCDD6F4;
            p.draw_text(44, iy, st.s);
            iy += FONT_H + 4;
        }
        p.transparent_bg = false;
    }
    win_flush(w1);

    int w2 = win_create(500, 50, 460, 340, "Graphics Test");
    win_focus(w2);
    {
        Painter& p = win_painter(w2);
        Rect cr(0, 0, 460 - BORDER_W*2, 340 - TITLEBAR_H - BORDER_W);

        p.color = WinColor::ClientBg;
        p.fill_rect(cr);

        Color32 pal[] = {
            0xFFFF5555, 0xFF50FA7B, 0xFF8BE9FD,
            0xFFBD93F9, 0xFFF1FA8C, 0xFFFF79C6,
        };
        const int N = 6;

        p.transparent_bg = true;

        p.color = 0xFF6C7086; p.draw_text(12, 8, "fill_rect");
        for (int i = 0; i < N; i++) {
            p.color = pal[i];
            p.fill_rect(Rect(12 + i*64, 26, 56, 28));
        }

        p.color = 0xFF6C7086; p.draw_text(12, 62, "fill_rounded_rect  r=10");
        for (int i = 0; i < N; i++) {
            p.color = pal[i];
            p.fill_rounded_rect(Rect(12 + i*64, 80, 56, 28), 10);
        }

        p.color = 0xFF6C7086; p.draw_text(12, 116, "fill_circle");
        for (int i = 0; i < N; i++) {
            p.color = pal[i];
            p.fill_circle(40 + i*64, 148, 20);
        }

        p.color = 0xFF6C7086; p.draw_text(12, 176, "draw_line");
        for (int i = 0; i < N; i++) {
            p.color = pal[i];
            p.draw_line(12 + i*64, 196, 60 + i*64, 216);
        }

        p.color = 0xFF6C7086; p.draw_text(12, 224, "gradient_h");
        p.fill_rect_gradient_h(Rect(12, 242, cr.w - 24, 18), 0xFFFF5555, 0xFF8BE9FD);

        p.color = 0xFF6C7086; p.draw_text(12, 266, "gradient_v");
        p.fill_rect_gradient_v(Rect(12, 284, cr.w - 24, 18), 0xFF50FA7B, 0xFFBD93F9);

        p.transparent_bg = false;
    }
    win_flush(w2);

    int sw = gfx_width(), sh = gfx_height();
    dp.color = 0xFF1A1A28;
    dp.fill_rect(Rect(0, sh - 32, sw, 32));
    dp.color = 0xFF44475A;
    dp.draw_hline(0, sh - 32, sw);
    dp.color = 0xFF6C7086;
    dp.transparent_bg = true;
    dp.draw_text_aligned(Rect(sw/2 - 80, sh-32, 160, 32),
                         "SabakaOS v0.4.3", Align::Center);
    dp.transparent_bg = false;

    gfx_flip();
}

extern "C" void kernel_main(uint32_t mb_magic, MultibootInfo* mb_info) {

    vga_cls();
    vga_fill_row(0, 4);
    vga_print("SabakaOS booting...", 0, 0, 15, 4);

    gdt_init();
    idt_init();
    pmm_init(32 * 1024 * 1024);

    bool have_fb = (mb_magic == 0x2BADB002) && fb_init(mb_info);

    paging_init();

    if (have_fb) {
        uint32_t base = fb_phys_addr() & ~0xFFFu;
        uint32_t end  = (fb_phys_addr() + fb_size_bytes() + 0xFFF) & ~0xFFFu;
        paging_map_region(base, base, end - base, PAGE_PRESENT | PAGE_WRITE);
    } else {
        vga_cls();
        vga_fill_row(0, 1);
        vga_print("  SabakaOS v0.4.3  [VGA text - no VESA]", 0, 0, 15, 1);
    }

    bool heap_ok = paging_alloc_region(HEAP_VIRT, HEAP_SIZE, PAGE_PRESENT | PAGE_WRITE);
    if (heap_ok) heap_init(HEAP_VIRT, HEAP_SIZE);

    if (have_fb) {
        gfx_init();
        graphics_init();
        gfx_test();
    }

    tss_init();
    pit_init(1000);
    vfs_init();

    if (rtl8139_init()) {
        net_init(ip_from_str("10.0.2.15"),
                 ip_from_str("10.0.2.2"),
                 ip_from_str("255.255.255.0"));
    }

    terminal_init();

    if (ata_init()) {
        terminal_set_color_fg(10);
        terminal_puts("[ATA] Disk found, ");
        char sec_buf[16]; kuitoa(ata_sectors_count(), sec_buf, 10);
        terminal_puts(sec_buf); terminal_puts(" sectors\n");
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
            terminal_puts("[ATA] FAT32 not found\n");
            terminal_reset_color();
        }
    } else {
        terminal_set_color_fg(14);
        terminal_puts("[ATA] No disk\n");
        terminal_reset_color();
    }

    uhci_init();
    hid_init();

    keyboard_init();
    keyboard_set_callback(terminal_on_key);

    if (have_fb) {
        mouse_init(gfx_width(), gfx_height());
        mouse_set_callback([](const MouseState& ms) {
            cursor_draw(ms.x, ms.y);
        });
        cursor_draw(gfx_width() / 2, gfx_height() / 2);
    }
    syscall_init();
    pipe_init_all();
    mutex_init(&shared_mutex);
    scheduler_init();

    process_create(mutex_proc_a, nullptr, "mutex_a", 5);
    process_create(mutex_proc_b, nullptr, "mutex_b", 5);

    shell_init();
    //terminal_set_execute_cb(shell_execute);
    //keyboard_set_callback(terminal_on_key);
    //terminal_reply_input();

    __asm__ volatile("sti");
    for(;;) __asm__ volatile("hlt");
}