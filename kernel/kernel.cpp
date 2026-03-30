#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "kstring.h"

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
static void vga_clear(Color bg=BLACK) {
    for(int i=0;i<COLS*ROWS;i++) VGA[i]=ve(' ',WHITE,bg);
}
static void vga_print(const char* s, int row, int col, Color fg=WHITE, Color bg=BLACK) {
    for(int i=0;s[i]&&col+i<COLS;i++) VGA[row*COLS+col+i]=ve(s[i],fg,bg);
}
static void vga_fill(char c, int row, Color fg, Color bg=BLACK) {
    for(int i=0;i<COLS;i++) VGA[row*COLS+i]=ve(c,fg,bg);
}
static void vga_clear_row(int row, Color bg=BLACK) {
    for(int i=0;i<COLS;i++) VGA[row*COLS+i]=ve(' ',WHITE,bg);
}
static void ok(const char* msg, int row) {
    vga_print("[  OK  ]",row,1,LIGHT_GREEN);
    vga_print(msg,row,10,WHITE);
}
static void vga_print_uint(uint32_t n, int row, int col, Color fg=WHITE) {
    char buf[12]; kuitoa(n, buf, 10);
    vga_print(buf, row, col, fg);
}

static const int TERM_TOP = 16, TERM_ROWS = 8;
static int trow=0, tcol=0;

static void term_scroll() {
    for(int r=TERM_TOP;r<TERM_TOP+TERM_ROWS-1;r++)
        for(int c=0;c<COLS;c++)
            VGA[r*COLS+c]=VGA[(r+1)*COLS+c];
    vga_clear_row(TERM_TOP+TERM_ROWS-1);
}
static void term_putchar(char ch) {
    if(ch=='\n'||tcol>=COLS){ tcol=0; trow++; if(trow>=TERM_ROWS){term_scroll();trow=TERM_ROWS-1;} if(ch=='\n')return; }
    if(ch==8){ if(tcol>0){ tcol--; VGA[(TERM_TOP+trow)*COLS+tcol]=ve(' ',WHITE,BLACK); } return; }
    VGA[(TERM_TOP+trow)*COLS+tcol]=ve(ch,LIGHT_GREEN,BLACK);
    tcol++;
}
static void term_puts(const char* s){ while(*s) term_putchar(*s++); }
static void term_cursor(bool on){
    VGA[(TERM_TOP+trow)*COLS+tcol]=ve(on?'_':' ',YELLOW,BLACK);
}

static void on_key(char c){
    term_cursor(false);
    term_putchar(c);
    if(c=='\n') term_puts("sabaka> ");
    term_cursor(true);
}

static void update_mem() {
    vga_clear_row(13);
    char buf[12];
    vga_print("MEM  phys free:", 13, 1, DARK_GREY);
    vga_print(kuitoa(pmm_free_pages()*4, buf, 10), 13, 16, LIGHT_CYAN);
    vga_print("KB  used:", 13, 22, DARK_GREY);
    vga_print(kuitoa(pmm_used_pages()*4, buf, 10), 13, 31, LIGHT_CYAN);
    vga_print("KB  heap free:", 13, 37, DARK_GREY);
    vga_print(kuitoa(heap_free()/1024,   buf, 10), 13, 51, LIGHT_CYAN);
    vga_print("KB", 13, 53+kstrlen(buf), DARK_GREY);
}

#define HEAP_VIRT  0x00800000
#define HEAP_SIZE  (4 * 1024 * 1024)

extern "C" void kernel_main() {
    vga_clear();

    vga_fill(' ',0,WHITE,BLUE);
    vga_print("  SabakaOS v0.0.6",0,0,WHITE,BLUE);
    vga_print("[x86 | Protected Mode | VGA 80x25]",0,44,YELLOW,BLUE);

    gdt_init();
    ok("GDT: 3 descriptors",2);

    idt_init();
    ok("IDT: 48 gates, PIC remapped, STI",3);

    keyboard_init();
    keyboard_set_callback(on_key);
    ok("Keyboard: PS/2 IRQ1",4);

    pmm_init(32 * 1024 * 1024);
    ok("PMM: 32MB physical memory",5);

    paging_init();
    ok("Paging: enabled, identity map 0-4MB",6);

    bool heap_mapped = paging_alloc_region(HEAP_VIRT, HEAP_SIZE,
                                           PAGE_PRESENT | PAGE_WRITE);
    if (heap_mapped) {
        heap_init(HEAP_VIRT, HEAP_SIZE);
        ok("Heap: 4MB @ 0x00800000 (paging_alloc_region)",7);
    } else {
        vga_print("[ FAIL ] Heap: OOM during paging_alloc_region",7,1,LIGHT_RED);
    }

    char* s1 = (char*)kmalloc(32);
    char* s2 = (char*)kmalloc(32);
    kstrcpy(s1, "Sabaka");
    kstrcpy(s2, "Lang");
    kstrcat(s1, s2);
    bool str_ok = (kstrcmp(s1, "SabakaLang") == 0);
    kfree(s1); kfree(s2);
    if (str_ok) ok("kstring: kmalloc+strcpy+strcat+strcmp — passed",8);
    else vga_print("[ FAIL ] kstring test",8,1,LIGHT_RED);

    char* r = (char*)kmalloc(8);
    kstrcpy(r, "hello");
    r = (char*)krealloc(r, 64);
    kstrcat(r, " world");
    bool realloc_ok = (kstrcmp(r, "hello world") == 0);
    kfree(r);
    if (realloc_ok) ok("krealloc: grow + memcpy — passed",9);
    else vga_print("[ FAIL ] krealloc test",9,1,LIGHT_RED);

    struct Point { int x, y; };
    Point* p = new Point{42, 7};
    bool new_ok = (p && p->x == 42 && p->y == 7);
    delete p;
    if (new_ok) ok("C++ new/delete Point{42,7} — passed",10);
    else vga_print("[ FAIL ] new/delete test",10,1,LIGHT_RED);

    vga_fill('-',12,DARK_GREY);
    vga_print("Memory:",12,1,YELLOW);
    update_mem();
    vga_fill('-',14,DARK_GREY);
    vga_print("Terminal:",15,1,YELLOW);
    vga_fill('-',TERM_TOP-1,DARK_GREY);

    term_puts("sabaka> ");
    term_cursor(true);

    vga_fill(' ',24,WHITE,BLUE);
    vga_print("  SabakaOS v0.0.6 | kstring + paging_alloc + krealloc",24,0,WHITE,BLUE);

    for(;;) __asm__ volatile("hlt");
}