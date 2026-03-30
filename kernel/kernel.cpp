#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "kstring.h"
#include "terminal.h"
#include "shell.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int COLS = 80;

static inline unsigned short ve(char c, unsigned char fg, unsigned char bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}
static void vga_print(const char* s, int row, int col,
                      unsigned char fg=15, unsigned char bg=0) {
    for(int i=0;s[i]&&col+i<COLS;i++)
        VGA[row*COLS+col+i]=ve(s[i],fg,bg);
}
static void vga_fill(char c, int row, unsigned char fg, unsigned char bg=0) {
    for(int i=0;i<COLS;i++) VGA[row*COLS+i]=ve(c,fg,bg);
}
static void ok(const char* msg, int row) {
    vga_print("[  OK  ]",row,0,10,0);
    vga_print(msg,row,9,15,0);
}
static void fail(const char* msg, int row) {
    vga_print("[ FAIL ]",row,0,12,0);
    vga_print(msg,row,9,15,0);
}

#define HEAP_VIRT (8 * 1024 * 1024)
#define HEAP_SIZE (4 * 1024 * 1024)

extern "C" void kernel_main() {
    for(int i=0;i<80*25;i++) VGA[i]=ve(' ',15,0);

    vga_fill(' ',0,15,1);
    vga_print("  SabakaOS v0.0.7",0,0,15,1);
    vga_print("[x86 | Protected Mode | VGA 80x25]",0,44,14,1);

    gdt_init();

    idt_init();

    pmm_init(32 * 1024 * 1024);

    paging_init();

    bool heap_ok = paging_alloc_region(HEAP_VIRT, HEAP_SIZE,
                                       PAGE_PRESENT | PAGE_WRITE);
    if (heap_ok) heap_init(HEAP_VIRT, HEAP_SIZE);

    keyboard_init();

    vga_fill(' ', 24, 15, 1);
    vga_print("  SabakaOS v0.0.7 | Shell ready | type 'help'", 24, 0, 15, 1);

    terminal_init();
    shell_init();

    terminal_set_execute_cb(shell_execute);

    keyboard_set_callback(terminal_on_key);

    terminal_set_color_fg(11);
    terminal_puts("Welcome to SabakaOS!\n");
    terminal_reset_color();
    terminal_puts("Type ");
    terminal_set_color_fg(10);
    terminal_puts("help");
    terminal_reset_color();
    terminal_puts(" to see available commands.");

    terminal_reply_input();

    for(;;) __asm__ volatile("hlt");
}