#include "gdt.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;

enum Color : unsigned char {
    BLACK         = 0,
    BLUE          = 1,
    GREEN         = 2,
    CYAN          = 3,
    RED           = 4,
    MAGENTA       = 5,
    BROWN         = 6,
    LIGHT_GREY    = 7,
    DARK_GREY     = 8,
    LIGHT_BLUE    = 9,
    LIGHT_GREEN   = 10,
    LIGHT_CYAN    = 11,
    LIGHT_RED     = 12,
    LIGHT_MAGENTA = 13,
    YELLOW        = 14,
    WHITE         = 15,
};

static inline unsigned short vga_entry(char c, Color fg, Color bg) {
    return (unsigned short)c | ((unsigned short)((bg << 4) | fg) << 8);
}

static void clear(Color bg = BLACK) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA[i] = vga_entry(' ', WHITE, bg);
}

static void print(const char* str, int row, int col,
                  Color fg = WHITE, Color bg = BLACK) {
    for (int i = 0; str[i]; i++)
        VGA[row * VGA_COLS + col + i] = vga_entry(str[i], fg, bg);
}

static void fill_row(char c, int row, Color fg, Color bg = BLACK) {
    for (int col = 0; col < VGA_COLS; col++)
        VGA[row * VGA_COLS + col] = vga_entry(c, fg, bg);
}

static void print_status(const char* msg, int row, bool ok = true) {
    if (ok) print("[  OK  ]", row, 1, LIGHT_GREEN);
    else    print("[ FAIL ]", row, 1, LIGHT_RED);
    print(msg, row, 10, WHITE);
}

extern "C" void kernel_main() {
    clear(BLACK);

    fill_row(' ', 0, WHITE, BLUE);
    print("  SabakaOS v0.0.2", 0, 0, WHITE, BLUE);
    print("[x86 | Protected Mode | VGA 80x25]", 0, 44, YELLOW, BLUE);

    gdt_init();

    print_status("Bootloader: GRUB Multiboot",                2);
    print_status("CPU: x86 32-bit Protected Mode",            3);
    print_status("VGA: Text mode 80x25",                      4);
    print_status("GDT: 3 descriptors (null / code / data)",   5);

    print("--------------------------------------------------------------------------------",
          7, 0, DARK_GREY);

    print("Welcome to SabakaOS!", 9, 30, YELLOW);
    print("Build: " __DATE__ " " __TIME__, 10, 30, DARK_GREY);
    print("Next: IDT + interrupts...", 12, 2, LIGHT_CYAN);

    fill_row(' ', 24, WHITE, BLUE);
    print("  SabakaOS Kernel | Halted", 24, 0, WHITE, BLUE);

    for (;;) __asm__ volatile("hlt");
}