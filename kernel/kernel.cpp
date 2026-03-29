#include "gdt.h"
#include "idt.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int COLS = 80, ROWS = 25;

enum Color : unsigned char {
    BLACK=0,BLUE=1,GREEN=2,CYAN=3,RED=4,MAGENTA=5,BROWN=6,
    LIGHT_GREY=7,DARK_GREY=8,LIGHT_BLUE=9,LIGHT_GREEN=10,
    LIGHT_CYAN=11,LIGHT_RED=12,LIGHT_MAGENTA=13,YELLOW=14,WHITE=15
};

static inline unsigned short ve(char c, Color fg, Color bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}
static void clear(Color bg=BLACK) {
    for (int i=0;i<COLS*ROWS;i++) VGA[i]=ve(' ',WHITE,bg);
}
static void print(const char* s, int row, int col, Color fg=WHITE, Color bg=BLACK) {
    for (int i=0;s[i];i++) VGA[row*COLS+col+i]=ve(s[i],fg,bg);
}
static void fill(char c, int row, Color fg, Color bg=BLACK) {
    for (int i=0;i<COLS;i++) VGA[row*COLS+i]=ve(c,fg,bg);
}
static void ok(const char* msg, int row) {
    print("[  OK  ]", row, 1, LIGHT_GREEN);
    print(msg, row, 10, WHITE);
}

extern "C" void kernel_main() {
    clear();

    fill(' ', 0, WHITE, BLUE);
    print("  SabakaOS v0.0.3", 0, 0, WHITE, BLUE);
    print("[x86 | Protected Mode | VGA 80x25]", 0, 44, YELLOW, BLUE);

    gdt_init();
    ok("GDT: 3 descriptors (null / code / data)", 2);

    idt_init();
    ok("IDT: 48 gates loaded, PIC remapped (IRQ -> 32-47)", 3);
    ok("Interrupts: enabled (STI)", 4);

    print("--------------------------------------------------------------------------------", 6, 0, DARK_GREY);
    print("Welcome to SabakaOS!", 8, 30, YELLOW);
    print("Build: " __DATE__ " " __TIME__, 9, 30, DARK_GREY);
    print("Next: Keyboard driver...", 11, 2, LIGHT_CYAN);

    fill(' ', 24, WHITE, BLUE);
    print("  SabakaOS Kernel | Running", 24, 0, WHITE, BLUE);

    for (;;) __asm__ volatile("hlt");
}