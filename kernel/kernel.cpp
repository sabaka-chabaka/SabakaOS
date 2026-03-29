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
    unsigned char color = (unsigned char)((bg << 4) | fg);
    return (unsigned short)c | ((unsigned short)color << 8);
}

static void clear(Color bg = BLACK) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA[i] = vga_entry(' ', WHITE, bg);
}

static void print(const char* str, int row, int col,
                  Color fg = WHITE, Color bg = BLACK) {
    for (int i = 0; str[i] != '\0'; i++)
        VGA[row * VGA_COLS + col + i] = vga_entry(str[i], fg, bg);
}

static void putchar(char c, int row, int col, Color fg, Color bg = BLACK) {
    VGA[row * VGA_COLS + col] = vga_entry(c, fg, bg);
}

static void fill_row(char c, int row, Color fg, Color bg = BLACK) {
    for (int col = 0; col < VGA_COLS; col++)
        VGA[row * VGA_COLS + col] = vga_entry(c, fg, bg);
}

static void print_int(int n, int row, int col, Color fg = WHITE) {
    if (n == 0) {
        putchar('0', row, col, fg);
        return;
    }
    char buf[12];
    int i = 0;
    bool neg = false;
    if (n < 0) { neg = true; n = -n; }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    if (neg) buf[i++] = '-';
    for (int j = i - 1; j >= 0; j--)
        putchar(buf[j], row, col++, fg);
}

extern "C" void kernel_main() {

    clear(BLACK);

    fill_row(' ', 0, WHITE, BLUE);
    print("  SabakaOS v0.0.1", 0, 0, WHITE, BLUE);
    print("[x86 | Protected Mode | VGA 80x25]", 0, 44, YELLOW, BLUE);

    print("[  OK  ]", 2, 1, LIGHT_GREEN);
    print("Bootloader: GRUB Multiboot", 2, 10, WHITE);

    print("[  OK  ]", 3, 1, LIGHT_GREEN);
    print("CPU: x86 32-bit Protected Mode", 3, 10, WHITE);

    print("[  OK  ]", 4, 1, LIGHT_GREEN);
    print("VGA: Text mode 80x25 initialized", 4, 10, WHITE);

    print("[  OK  ]", 5, 1, LIGHT_GREEN);
    print("Stack: 16 KiB at 0x", 5, 10, WHITE);
    print("kernel_main: reached", 5, 30, LIGHT_CYAN);

    print("--------------------------------------------------------------------------------",
          7, 0, DARK_GREY);

    print("Welcome to SabakaOS!", 9, 30, YELLOW);
    print("Build: " __DATE__ " " __TIME__, 10, 30, DARK_GREY);

    print("System halted. Nothing to do yet - but the kernel is alive!", 12, 2, LIGHT_CYAN);
    print("Next step: GDT, IDT, interrupts...", 13, 2, DARK_GREY);

    fill_row(' ', 24, WHITE, BLUE);
    print("  SabakaOS Kernel | Halted", 24, 0, WHITE, BLUE);

    for (;;)
        __asm__ volatile("hlt");
}