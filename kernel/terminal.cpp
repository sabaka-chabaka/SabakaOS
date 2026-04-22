#include "terminal.h"
#include "kstring.h"
#include "vesa.h"

static unsigned short* const VGA_MEM = (unsigned short*)0xB8000;
static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;

static inline unsigned short vga_entry(char c, uint8_t fg, uint8_t bg) {
    return (unsigned short)c | ((unsigned short)((bg << 4) | fg) << 8);
}

static const int CHAR_W = 8;
static const int CHAR_H = 16;

static const uint32_t PALETTE[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static int COLS           = 80;
static int ROWS           = 25;
static int TERM_ROW_START = 3;
static int TERM_ROW_END   = 24;

static uint8_t cur_fg = 15;
static uint8_t cur_bg = 0;
static int     cur_row = 3;
static int     cur_col = 0;

static void (*execute_cb)(const char*) = nullptr;
void terminal_set_execute_cb(void (*cb)(const char*)) { execute_cb = cb; }

struct Cell { char c; uint8_t fg; uint8_t bg; };
static const int MAX_COLS = 200;
static const int MAX_ROWS = 80;
static Cell shadow[MAX_ROWS][MAX_COLS];

static void draw_cell(int row, int col, char c, uint8_t fg, uint8_t bg) {
    if (vesa_available()) {
        vesa_draw_char(col * CHAR_W, row * CHAR_H, c, PALETTE[fg & 15], PALETTE[bg & 15]);
    } else {
        if (row >= 0 && row < VGA_ROWS && col >= 0 && col < VGA_COLS)
            VGA_MEM[row * VGA_COLS + col] = vga_entry(c, fg, bg);
    }
}

static void put_cell(int row, int col, char c, uint8_t fg, uint8_t bg) {
    draw_cell(row, col, c, fg, bg);
    if (row >= 0 && row < MAX_ROWS && col >= 0 && col < MAX_COLS)
        shadow[row][col] = {c, fg, bg};
}

static void do_scroll() {
    if (vesa_available()) {
        // Shift shadow up one row and redraw
        for (int r = TERM_ROW_START; r < TERM_ROW_END - 1; r++)
            for (int c = 0; c < COLS; c++)
                shadow[r][c] = shadow[r + 1][c];
        for (int c = 0; c < COLS; c++)
            shadow[TERM_ROW_END - 1][c] = {' ', 15, 0};
        for (int r = TERM_ROW_START; r < TERM_ROW_END; r++)
            for (int c = 0; c < COLS; c++)
                draw_cell(r, c, shadow[r][c].c, shadow[r][c].fg, shadow[r][c].bg);
    } else {
        for (int r = TERM_ROW_START; r < TERM_ROW_END - 1; r++)
            for (int c = 0; c < VGA_COLS; c++)
                VGA_MEM[r * VGA_COLS + c] = VGA_MEM[(r + 1) * VGA_COLS + c];
        for (int c = 0; c < VGA_COLS; c++)
            VGA_MEM[(TERM_ROW_END - 1) * VGA_COLS + c] = vga_entry(' ', 15, 0);
    }
}

static void cursor_show(bool on) {
    if (cur_row < TERM_ROW_END)
        put_cell(cur_row, cur_col, on ? '_' : ' ', on ? 14 : 15, 0);
}

void terminal_putchar(char c) {
    cursor_show(false);
    if (c == '\n') {
        cur_col = 0; cur_row++;
        if (cur_row >= TERM_ROW_END) { do_scroll(); cur_row = TERM_ROW_END - 1; }
    } else if (c == '\t') {
        int tab = ((cur_col / 4) + 1) * 4;
        while (cur_col < tab && cur_col < COLS - 1) {
            put_cell(cur_row, cur_col, ' ', cur_fg, cur_bg);
            cur_col++;
        }
    } else {
        put_cell(cur_row, cur_col, c, cur_fg, cur_bg);
        cur_col++;
        if (cur_col >= COLS) {
            cur_col = 0; cur_row++;
            if (cur_row >= TERM_ROW_END) { do_scroll(); cur_row = TERM_ROW_END - 1; }
        }
    }
    cursor_show(true);
}

void terminal_puts(const char* s)         { while (*s) terminal_putchar(*s++); }
void terminal_put_uint(uint32_t n, int b) { char buf[34]; kuitoa(n, buf, b); terminal_puts(buf); }
void terminal_put_int(int32_t n)          { char buf[12]; kitoa(n, buf, 10); terminal_puts(buf); }
void terminal_newline()                   { terminal_putchar('\n'); }
void terminal_set_color_fg(uint8_t fg)    { cur_fg = fg; }
void terminal_reset_color()               { cur_fg = 15; cur_bg = 0; }

void terminal_clear() {
    for (int r = TERM_ROW_START; r < TERM_ROW_END; r++)
        for (int c = 0; c < COLS; c++)
            put_cell(r, c, ' ', 15, 0);
    cur_row = TERM_ROW_START;
    cur_col = 0;
}

void terminal_init() {
    if (vesa_available()) {
        COLS           = vesa_width()  / CHAR_W;
        ROWS           = vesa_height() / CHAR_H;
        TERM_ROW_START = 2;
        TERM_ROW_END   = ROWS - 1;
        vesa_fill_rect(0, 0, vesa_width(), vesa_height(), 0x000000);
        for (int r = 0; r < MAX_ROWS; r++)
            for (int c = 0; c < MAX_COLS; c++)
                shadow[r][c] = {' ', 15, 0};
    } else {
        COLS           = VGA_COLS;
        ROWS           = VGA_ROWS;
        TERM_ROW_START = 3;
        TERM_ROW_END   = 24;
        for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
            VGA_MEM[i] = vga_entry(' ', 15, 0);
    }
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
static int  prompt_row = 3;
static int  prompt_col = 0;
static char prompt_path[256] = "/";

void terminal_set_prompt_path(const char* path) {
    uint32_t i = 0;
    while (path[i] && i < sizeof(prompt_path) - 1) { prompt_path[i] = path[i]; i++; }
    prompt_path[i] = 0;
}

static void print_prompt() {
    cursor_show(false);
    cur_fg = 11; terminal_puts("sabaka");
    cur_fg = 15; terminal_putchar('@');
    cur_fg = 10; terminal_puts("SabakaOS");
    cur_fg = 15; terminal_puts(": ");
    cur_fg = 11; terminal_puts("~");
    if (prompt_path[0] == '/' && prompt_path[1] != 0)
        terminal_puts(prompt_path);
    cur_fg = 15; terminal_puts("> ");
}

static void input_redraw() {
    cursor_show(false);
    int r = prompt_row, c = prompt_col;
    for (int i = 0; i < input_len; i++) {
        put_cell(r, c, input_buf[i], 10, 0);
        if (++c >= COLS) { c = 0; r++; }
    }
    for (int fc = c; fc < COLS; fc++)
        put_cell(r, fc, ' ', 15, 0);
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

static void input_clear_line() {
    cursor_show(false);
    int r = prompt_row, c = prompt_col;
    for (int i = 0; i < input_len; i++) {
        put_cell(r, c, ' ', 15, 0);
        if (++c >= COLS) { c = 0; r++; }
    }
    input_len = 0;
    input_pos = 0;
}

void terminal_on_key(char c) {
    if (c == 17) { // UP
        if (hist_count > 0) {
            if (hist_idx == -1) hist_idx = hist_count - 1;
            else if (hist_idx > 0) hist_idx--;
            input_clear_line();
            kstrcpy(input_buf, history[hist_idx]);
            input_len = kstrlen(input_buf);
            input_pos = input_len;
            input_redraw();
        }
        return;
    }
    if (c == 18) { // DOWN
        if (hist_idx != -1) {
            hist_idx++;
            input_clear_line();
            if (hist_idx < hist_count)
                kstrcpy(input_buf, history[hist_idx]);
            else { hist_idx = -1; kmemset(input_buf, 0, INPUT_MAX); }
            input_len = kstrlen(input_buf);
            input_pos = input_len;
            input_redraw();
        }
        return;
    }
    if (c == '\n') {
        cursor_show(false);
        cur_row = prompt_row;
        cur_col = prompt_col + input_len;
        while (cur_col >= COLS) { cur_col -= COLS; cur_row++; }
        terminal_newline();
        input_buf[input_len] = 0;
        if (input_len > 0) {
            int idx = hist_count < HIST_MAX ? hist_count++ : HIST_MAX - 1;
            if (idx == HIST_MAX - 1)
                for (int i = 0; i < HIST_MAX - 1; i++) kstrcpy(history[i], history[i + 1]);
            kstrcpy(history[idx], input_buf);
        }
        if (execute_cb) execute_cb(input_buf);
        start_input();
    } else if (c == 8) {
        if (input_pos > 0) {
            for (int i = input_pos - 1; i < input_len - 1; i++)
                input_buf[i] = input_buf[i + 1];
            input_len--; input_pos--;
            input_buf[input_len] = 0;
            input_redraw();
        }
    } else if (c >= 32 && c < 127) {
        if (input_len < INPUT_MAX - 1) {
            for (int i = input_len; i > input_pos; i--)
                input_buf[i] = input_buf[i - 1];
            input_buf[input_pos++] = c;
            input_len++;
            input_buf[input_len] = 0;
            input_redraw();
        }
    }
}

void terminal_reply_input() { start_input(); }