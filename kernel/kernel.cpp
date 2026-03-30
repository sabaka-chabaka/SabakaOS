#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"

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

static void print_uint(uint32_t n, int row, int col, Color fg=WHITE) {
    if (n == 0) { VGA[row*COLS+col] = ve('0', fg, BLACK); return; }
    char buf[12]; int i=0;
    while(n>0){ buf[i++]='0'+(n%10); n/=10; }
    for(int j=i-1;j>=0;j--)
        VGA[row*COLS+col+(i-1-j)] = ve(buf[j], fg, BLACK);
}

static int  term_row=0, term_col=0;
static const int TERM_TOP=16, TERM_ROWS=8;

static void term_scroll() {
    for(int r=TERM_TOP;r<TERM_TOP+TERM_ROWS-1;r++)
        for(int c=0;c<COLS;c++)
            VGA[r*COLS+c]=VGA[(r+1)*COLS+c];
    for(int c=0;c<COLS;c++)
        VGA[(TERM_TOP+TERM_ROWS-1)*COLS+c]=ve(' ',WHITE,BLACK);
}
static void term_putchar(char ch) {
    if(ch=='\n'||term_col>=COLS){
        term_col=0; term_row++;
        if(term_row>=TERM_ROWS){ term_scroll(); term_row=TERM_ROWS-1; }
        if(ch=='\n') return;
    }
    if(ch==8){ if(term_col>0){ term_col--; VGA[(TERM_TOP+term_row)*COLS+term_col]=ve(' ',WHITE,BLACK); } return; }
    VGA[(TERM_TOP+term_row)*COLS+term_col]=ve(ch,LIGHT_GREEN,BLACK);
    term_col++;
}
static void term_puts(const char* s){ for(int i=0;s[i];i++) term_putchar(s[i]); }
static void term_cursor(bool show){
    VGA[(TERM_TOP+term_row)*COLS+term_col]=ve(show?'_':' ', YELLOW, BLACK);
}

static void on_key(char c){
    term_cursor(false);
    term_putchar(c);
    if(c=='\n') term_puts("sabaka> ");
    term_cursor(true);
}

static void update_mem_status() {
    for(int i=0;i<COLS;i++) VGA[13*COLS+i]=ve(' ',WHITE,BLACK);
    print("MEM  free:", 13, 1, DARK_GREY);
    print_uint(pmm_free_pages()*4, 13, 11, LIGHT_CYAN);
    print("KB  used:", 13, 17, DARK_GREY);
    print_uint(pmm_used_pages()*4, 13, 26, LIGHT_CYAN);
    print("KB  heap:", 13, 32, DARK_GREY);
    print_uint(heap_free()/1024, 13, 41, LIGHT_CYAN);
    print("KB free", 13, 45, DARK_GREY);
}

extern "C" void kernel_main() {
    clear();

    fill(' ', 0, WHITE, BLUE);
    print("  SabakaOS v0.0.5", 0, 0, WHITE, BLUE);
    print("[x86 | Protected Mode | VGA 80x25]", 0, 44, YELLOW, BLUE);

    gdt_init();
    ok("GDT: 3 descriptors (null / code / data)", 2);

    idt_init();
    ok("IDT: 48 gates, PIC remapped, STI", 3);

    keyboard_init();
    keyboard_set_callback(on_key);
    ok("Keyboard: PS/2 IRQ1 driver loaded", 4);

    pmm_init(32 * 1024 * 1024);
    ok("PMM: physical memory manager initialized", 5);

    paging_init();
    ok("Paging: enabled, identity map 0-4MB", 6);

    heap_init(8 * 1024 * 1024, 2 * 1024 * 1024);
    ok("Heap: 2MB kmalloc/kfree ready (new/delete ok)", 7);

    int* test = new int(42);
    bool heap_ok = (test != nullptr && *test == 42);
    delete test;
    if (heap_ok) ok("Heap test: new int(42), delete — passed", 8);
    else         { print("[ FAIL ] Heap test failed!", 8, 1, LIGHT_RED); }

    print("--------------------------------------------------------------------------------", 10, 0, DARK_GREY);
    print("Memory status:", 11, 1, YELLOW);
    update_mem_status();
    print("--------------------------------------------------------------------------------", 14, 0, DARK_GREY);
    print("Terminal:", 15, 1, YELLOW);
    print("--------------------------------------------------------------------------------", TERM_TOP-1, 0, DARK_GREY);

    term_puts("sabaka> ");
    term_cursor(true);

    fill(' ', 24, WHITE, BLUE);
    print("  SabakaOS v0.0.5 | PMM + Paging + Heap", 24, 0, WHITE, BLUE);

    for(;;) __asm__ volatile("hlt");
}