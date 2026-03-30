#include "terminal.h"
#include "kstring.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int COLS = 80;
static const int TERM_ROW_START = 3;
static const int TERM_ROW_END   = 24;

enum {
    C_BLACK=0,C_BLUE=1,C_GREEN=2,C_CYAN=3,C_RED=4,
    C_MAGENTA=5,C_BROWN=6,C_LGREY=7,C_DGREY=8,
    C_LBLUE=9,C_LGREEN=10,C_LCYAN=11,C_LRED=12,
    C_LMAGENTA=13,C_YELLOW=14,C_WHITE=15
};

static uint8_t cur_fg = C_WHITE;
static uint8_t cur_bg = C_BLACK;
static int     cur_row = TERM_ROW_START;
static int     cur_col = 0;

static void (*execute_cb)(const char*) = nullptr;
void terminal_set_execute_cb(void (*cb)(const char*)) { execute_cb = cb; }

static inline unsigned short ve(char c, uint8_t fg, uint8_t bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}

static void scroll() {
    for (int r = TERM_ROW_START; r < TERM_ROW_END-1; r++)
        for (int c = 0; c < COLS; c++)
            VGA[r*COLS+c] = VGA[(r+1)*COLS+c];
    for (int c = 0; c < COLS; c++)
        VGA[(TERM_ROW_END-1)*COLS+c] = ve(' ', C_WHITE, C_BLACK);
}

static void cursor_show(bool on) {
    if (cur_row < TERM_ROW_END)
        VGA[cur_row*COLS+cur_col] = ve(on ? '_' : ' ', C_YELLOW, C_BLACK);
}

void terminal_putchar(char c) {
    cursor_show(false);
    if (c == '\n') {
        cur_col = 0; cur_row++;
        if (cur_row >= TERM_ROW_END) { scroll(); cur_row = TERM_ROW_END-1; }
    } else if (c == '\t') {
        int tab = ((cur_col/4)+1)*4;
        while (cur_col < tab && cur_col < COLS-1) {
            VGA[cur_row*COLS+cur_col] = ve(' ', cur_fg, cur_bg);
            cur_col++;
        }
    } else {
        VGA[cur_row*COLS+cur_col] = ve(c, cur_fg, cur_bg);
        cur_col++;
        if (cur_col >= COLS) {
            cur_col = 0; cur_row++;
            if (cur_row >= TERM_ROW_END) { scroll(); cur_row = TERM_ROW_END-1; }
        }
    }
    cursor_show(true);
}

void terminal_puts(const char* s)         { while(*s) terminal_putchar(*s++); }
void terminal_put_uint(uint32_t n, int b) { char buf[34]; kuitoa(n,buf,b); terminal_puts(buf); }
void terminal_put_int(int32_t n)          { char buf[12]; kitoa(n,buf,10); terminal_puts(buf); }
void terminal_newline()                   { terminal_putchar('\n'); }
void terminal_set_color_fg(uint8_t fg)    { cur_fg = fg; }
void terminal_reset_color()               { cur_fg = C_WHITE; cur_bg = C_BLACK; }

void terminal_clear() {
    for (int r = TERM_ROW_START; r < TERM_ROW_END; r++)
        for (int c = 0; c < COLS; c++)
            VGA[r*COLS+c] = ve(' ', C_WHITE, C_BLACK);
    cur_row = TERM_ROW_START;
    cur_col = 0;
}

static const int INPUT_MAX = 256;
static const int HIST_MAX  = 16;

static char input_buf[INPUT_MAX];
static int  input_len = 0;
static int  input_pos = 0;
static char history[HIST_MAX][INPUT_MAX];
static int  hist_count = 0;
static int  hist_idx   = -1;
static int  prompt_row = TERM_ROW_START;
static int  prompt_col = 0;

static void print_prompt() {
    cursor_show(false);
    cur_fg = C_LCYAN;  terminal_puts("sabaka");
    cur_fg = C_WHITE;  terminal_putchar('@');
    cur_fg = C_LGREEN; terminal_puts("SabakaOS");
    cur_fg = C_WHITE;  terminal_puts("> ");
    cur_fg = C_WHITE;
}

static void input_redraw() {
    cursor_show(false);
    int r = prompt_row, c = prompt_col;
    for (int i = 0; i < input_len; i++) {
        VGA[r*COLS+c] = ve(input_buf[i], C_LGREEN, C_BLACK);
        if (++c >= COLS) { c=0; r++; }
    }
    for (int fc = c; fc < COLS; fc++)
        VGA[r*COLS+fc] = ve(' ', C_WHITE, C_BLACK);
    cur_row = prompt_row;
    cur_col = prompt_col + input_pos;
    while (cur_col >= COLS) { cur_col -= COLS; cur_row++; }
    cursor_show(true);
}

static void start_input() {
    input_len = input_pos = 0;
    hist_idx = -1;
    kmemset(input_buf, 0, INPUT_MAX);
    print_prompt();
    prompt_row = cur_row;
    prompt_col = cur_col;
    cursor_show(true);
}

void terminal_on_key(char c) {
    if (c == '\n') {
        cursor_show(false);
        cur_row = prompt_row;
        cur_col = prompt_col + input_len;
        while (cur_col >= COLS) { cur_col -= COLS; cur_row++; }
        terminal_newline();
        input_buf[input_len] = 0;

        if (input_len > 0) {
            int idx = hist_count < HIST_MAX ? hist_count++ : HIST_MAX-1;
            if (idx == HIST_MAX-1)
                for (int i=0;i<HIST_MAX-1;i++) kstrcpy(history[i],history[i+1]);
            kstrcpy(history[idx], input_buf);
        }

        if (execute_cb) execute_cb(input_buf);
        start_input();

    } else if (c == 8) {
        if (input_pos > 0) {
            for (int i=input_pos-1;i<input_len-1;i++)
                input_buf[i]=input_buf[i+1];
            input_len--; input_pos--;
            input_buf[input_len]=0;
            input_redraw();
        }
    } else if (c >= 32 && c < 127) {
        if (input_len < INPUT_MAX-1) {
            for (int i=input_len;i>input_pos;i--)
                input_buf[i]=input_buf[i-1];
            input_buf[input_pos++]=c;
            input_len++;
            input_buf[input_len]=0;
            input_redraw();
        }
    }
}

void terminal_init() {
    terminal_clear();
}

void terminal_reply_input() {
    start_input();
}