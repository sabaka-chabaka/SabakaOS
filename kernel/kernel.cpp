#include "gdt.h"
#include "idt.h"
#include "keyboard.h"

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
    for(int i=0;i<COLS*ROWS;i++) VGA[i]=ve(' ',WHITE,bg);
}
static void print(const char* s, int row, int col, Color fg=WHITE, Color bg=BLACK) {
    for(int i=0;s[i];i++) VGA[row*COLS+col+i]=ve(s[i],fg,bg);
}
static void fill(char c, int row, Color fg, Color bg=BLACK) {
    for(int i=0;i<COLS;i++) VGA[row*COLS+i]=ve(c,fg,bg);
}
static void ok(const char* msg, int row) {
    print("[  OK  ]", row, 1, LIGHT_GREEN);
    print(msg,       row, 10, WHITE);
}

static int  term_row = 0;
static int  term_col = 0;
static const int TERM_TOP  = 14;
static const int TERM_ROWS = 10;

static void term_scroll() {
    for(int r = TERM_TOP; r < TERM_TOP + TERM_ROWS - 1; r++)
        for(int c = 0; c < COLS; c++)
            VGA[r*COLS+c] = VGA[(r+1)*COLS+c];
    for(int c=0;c<COLS;c++)
        VGA[(TERM_TOP+TERM_ROWS-1)*COLS+c] = ve(' ', WHITE, BLACK);
}

static void term_putchar(char ch) {
    if(ch == '\n' || term_col >= COLS) {
        term_col = 0;
        term_row++;
        if(term_row >= TERM_ROWS) {
            term_scroll();
            term_row = TERM_ROWS - 1;
        }
        if(ch == '\n') return;
    }
    if(ch == 8) {
        if(term_col > 0) {
            term_col--;
            VGA[(TERM_TOP+term_row)*COLS+term_col] = ve(' ', WHITE, BLACK);
        }
        return;
    }
    VGA[(TERM_TOP+term_row)*COLS+term_col] = ve(ch, LIGHT_GREEN, BLACK);
    term_col++;
}

static void term_puts(const char* s) {
    for(int i=0;s[i];i++) term_putchar(s[i]);
}

static void term_draw_cursor() {
    VGA[(TERM_TOP+term_row)*COLS+term_col] = ve('_', YELLOW, BLACK);
}
static void term_clear_cursor() {
    VGA[(TERM_TOP+term_row)*COLS+term_col] = ve(' ', WHITE, BLACK);
}

static void on_key(char c) {
    term_clear_cursor();
    term_putchar(c);
    term_draw_cursor();
}

extern "C" void kernel_main() {
    clear();

    fill(' ', 0, WHITE, BLUE);
    print("  SabakaOS v0.0.4", 0, 0, WHITE, BLUE);
    print("[x86 | Protected Mode | VGA 80x25]", 0, 44, YELLOW, BLUE);

    gdt_init();
    ok("GDT: 3 descriptors (null / code / data)", 2);

    idt_init();
    ok("IDT: 48 gates, PIC remapped, STI", 3);

    keyboard_init();
    keyboard_set_callback(on_key);
    ok("Keyboard: PS/2 IRQ1 driver loaded", 4);

    print("--------------------------------------------------------------------------------", 6, 0, DARK_GREY);
    print("SabakaOS Terminal", 8, 2, YELLOW);
    print("Type anything — keyboard is live!", 9, 2, DARK_GREY);
    print("--------------------------------------------------------------------------------", 11, 0, DARK_GREY);
    print("Hint: backspace works, enter adds new line", 12, 2, DARK_GREY);
    print("--------------------------------------------------------------------------------", 13, 0, DARK_GREY);

    term_draw_cursor();

    fill(' ', 24, WHITE, BLUE);
    print("  SabakaOS Kernel | Keyboard Active", 24, 0, WHITE, BLUE);

    for(;;) __asm__ volatile("hlt");
}