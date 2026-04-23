#include "terminal.h"
#include "kstring.h"
#include "fb.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int VGA_COLS = 80;
static const int TERM_ROW_START = 3;
static const int TERM_ROW_END   = 24;

static const int FB_FONT_W = 8;
static const int FB_FONT_H = 16;

static const int FB_TERM_Y0 = 20;

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

static int fb_cx = 0;
static int fb_cy = 0;

static void (*execute_cb)(const char*) = nullptr;
void terminal_set_execute_cb(void (*cb)(const char*)) { execute_cb = cb; }

static inline unsigned short ve(char c, uint8_t fg, uint8_t bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}

static void vga_scroll() {
    for (int r = TERM_ROW_START; r < TERM_ROW_END-1; r++)
        for (int c = 0; c < VGA_COLS; c++)
            VGA[r*VGA_COLS+c] = VGA[(r+1)*VGA_COLS+c];
    for (int c = 0; c < VGA_COLS; c++)
        VGA[(TERM_ROW_END-1)*VGA_COLS+c] = ve(' ', C_WHITE, C_BLACK);
}

static void vga_cursor_show(bool on) {
    if (cur_row < TERM_ROW_END)
        VGA[cur_row*VGA_COLS+cur_col] = ve(on ? '_' : ' ', C_YELLOW, C_BLACK);
}

static int fb_cols()  { return (int)fb_width()  / FB_FONT_W; }
static int fb_rows()  { return ((int)fb_height() - FB_TERM_Y0) / FB_FONT_H; }

static void fb_scroll_terminal() {
    fb_scroll(1);
    if (fb_cy >= FB_FONT_H) fb_cy -= FB_FONT_H;
}

static void fb_cursor_show(bool on) {
    int px = fb_cx;
    int py = FB_TERM_Y0 + fb_cy;
    if (on) {
        fb_fill_rect(px, py + FB_FONT_H - 2, FB_FONT_W, 2,
                     FB_PALETTE[C_YELLOW]);
    } else {
        fb_fill_rect(px, py + FB_FONT_H - 2, FB_FONT_W, 2,
                     FB_PALETTE[C_BLACK]);
    }
}

void terminal_putchar(char c) {
    if (fb_available()) {
        fb_cursor_show(false);

        if (c == '\n') {
            fb_cx = 0;
            fb_cy += FB_FONT_H;
            if (fb_cy + FB_FONT_H > (int)fb_height() - FB_TERM_Y0) {
                fb_scroll_terminal();
            }
        } else if (c == '\t') {
            int tab = ((fb_cx / FB_FONT_W / 4) + 1) * 4 * FB_FONT_W;
            while (fb_cx < tab && fb_cx < (int)fb_width() - FB_FONT_W) {
                fb_draw_char(fb_cx, FB_TERM_Y0 + fb_cy, ' ',
                             FB_PALETTE[cur_fg], FB_PALETTE[cur_bg]);
                fb_cx += FB_FONT_W;
            }
        } else if (c == '\b') {
            if (fb_cx >= FB_FONT_W) {
                fb_cx -= FB_FONT_W;
                fb_draw_char(fb_cx, FB_TERM_Y0 + fb_cy, ' ',
                             FB_PALETTE[cur_fg], FB_PALETTE[cur_bg]);
            }
        } else {
            fb_draw_char(fb_cx, FB_TERM_Y0 + fb_cy, c,
                         FB_PALETTE[cur_fg], FB_PALETTE[cur_bg]);
            fb_cx += FB_FONT_W;
            if (fb_cx + FB_FONT_W > (int)fb_width()) {
                fb_cx = 0;
                fb_cy += FB_FONT_H;
                if (fb_cy + FB_FONT_H > (int)fb_height() - FB_TERM_Y0) {
                    fb_scroll_terminal();
                }
            }
        }
        fb_cursor_show(true);
    } else {
        vga_cursor_show(false);
        if (c == '\n') {
            cur_col = 0; cur_row++;
            if (cur_row >= TERM_ROW_END) { vga_scroll(); cur_row = TERM_ROW_END-1; }
        } else if (c == '\t') {
            int tab = ((cur_col/4)+1)*4;
            while (cur_col < tab && cur_col < VGA_COLS-1) {
                VGA[cur_row*VGA_COLS+cur_col] = ve(' ', cur_fg, cur_bg);
                cur_col++;
            }
        } else {
            VGA[cur_row*VGA_COLS+cur_col] = ve(c, cur_fg, cur_bg);
            cur_col++;
            if (cur_col >= VGA_COLS) {
                cur_col = 0; cur_row++;
                if (cur_row >= TERM_ROW_END) { vga_scroll(); cur_row = TERM_ROW_END-1; }
            }
        }
        vga_cursor_show(true);
    }
}

void terminal_puts(const char* s)         { while(*s) terminal_putchar(*s++); }
void terminal_put_uint(uint32_t n, int b) { char buf[34]; kuitoa(n,buf,b); terminal_puts(buf); }
void terminal_put_int(int32_t n)          { char buf[12]; kitoa(n,buf,10); terminal_puts(buf); }
void terminal_newline()                   { terminal_putchar('\n'); }
void terminal_set_color_fg(uint8_t fg)    { cur_fg = fg; }
void terminal_reset_color()               { cur_fg = C_WHITE; cur_bg = C_BLACK; }

void terminal_clear() {
    if (fb_available()) {
        fb_fill_rect(0, FB_TERM_Y0, (int)fb_width(),
                     (int)fb_height() - FB_TERM_Y0, FB_PALETTE[C_BLACK]);
        fb_cx = 0;
        fb_cy = 0;
    } else {
        for (int r = TERM_ROW_START; r < TERM_ROW_END; r++)
            for (int c = 0; c < VGA_COLS; c++)
                VGA[r*VGA_COLS+c] = ve(' ', C_WHITE, C_BLACK);
        cur_row = TERM_ROW_START;
        cur_col = 0;
    }
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
static int  fb_prompt_x = 0;
static int  fb_prompt_y = 0;

static char prompt_path[256] = "/";

void terminal_set_prompt_path(const char* path) {
    uint32_t i = 0;
    while (path[i] && i < sizeof(prompt_path)-1) { prompt_path[i] = path[i]; i++; }
    prompt_path[i] = 0;
}

static void print_prompt() {
    if (fb_available()) fb_cursor_show(false);
    else                vga_cursor_show(false);

    cur_fg = C_LCYAN;  terminal_puts("sabaka");
    cur_fg = C_WHITE;  terminal_putchar('@');
    cur_fg = C_LGREEN; terminal_puts("SabakaOS");
    cur_fg = C_WHITE;  terminal_puts(": ");
    cur_fg = C_LCYAN;  terminal_puts("~");
    if (prompt_path[0] == '/' && prompt_path[1] != 0)
        terminal_puts(prompt_path);
    cur_fg = C_WHITE;  terminal_puts("> ");
    cur_fg = C_WHITE;
}

static void input_redraw_vga() {
    vga_cursor_show(false);
    int r = prompt_row, c = prompt_col;
    for (int i = 0; i < input_len; i++) {
        VGA[r*VGA_COLS+c] = ve(input_buf[i], C_LGREEN, C_BLACK);
        if (++c >= VGA_COLS) { c=0; r++; }
    }
    for (int fc = c; fc < VGA_COLS; fc++)
        VGA[r*VGA_COLS+fc] = ve(' ', C_WHITE, C_BLACK);
    cur_row = prompt_row;
    cur_col = prompt_col + input_pos;
    while (cur_col >= VGA_COLS) { cur_col -= VGA_COLS; cur_row++; }
    vga_cursor_show(true);
}

static void input_redraw_fb() {
    fb_cursor_show(false);
    int px = fb_prompt_x;
    int py = fb_prompt_y;
    for (int i = 0; i < input_len; i++) {
        fb_draw_char(px, FB_TERM_Y0 + py, input_buf[i],
                     FB_PALETTE[C_LGREEN], FB_PALETTE[C_BLACK]);
        px += FB_FONT_W;
        if (px + FB_FONT_W > (int)fb_width()) { px = 0; py += FB_FONT_H; }
    }
    int ex = px;
    while (ex + FB_FONT_W <= (int)fb_width()) {
        fb_fill_rect(ex, FB_TERM_Y0 + py, FB_FONT_W, FB_FONT_H, FB_PALETTE[C_BLACK]);
        ex += FB_FONT_W;
    }
    fb_cx = fb_prompt_x + input_pos * FB_FONT_W;
    fb_cy = fb_prompt_y;
    while (fb_cx + FB_FONT_W > (int)fb_width()) {
        fb_cx -= (int)fb_width();
        fb_cy += FB_FONT_H;
    }
    fb_cursor_show(true);
}

static void input_redraw() {
    if (fb_available()) input_redraw_fb();
    else                input_redraw_vga();
}

static void start_input() {
    input_len = input_pos = 0;
    hist_idx = -1;
    kmemset(input_buf, 0, INPUT_MAX);
    print_prompt();
    if (fb_available()) {
        fb_prompt_x = fb_cx;
        fb_prompt_y = fb_cy;
        fb_cursor_show(true);
    } else {
        prompt_row = cur_row;
        prompt_col = cur_col;
        vga_cursor_show(true);
    }
}

static void input_clear_line() {
    if (fb_available()) {
        int px = fb_prompt_x, py = fb_prompt_y;
        for (int i = 0; i < input_len; i++) {
            fb_fill_rect(px, FB_TERM_Y0 + py, FB_FONT_W, FB_FONT_H,
                         FB_PALETTE[C_BLACK]);
            px += FB_FONT_W;
            if (px + FB_FONT_W > (int)fb_width()) { px = 0; py += FB_FONT_H; }
        }
    } else {
        vga_cursor_show(false);
        int r = prompt_row, c = prompt_col;
        for (int i = 0; i < input_len; i++) {
            VGA[r*VGA_COLS+c] = ve(' ', C_WHITE, C_BLACK);
            if (++c >= VGA_COLS) { c=0; r++; }
        }
    }
    input_len = 0;
    input_pos = 0;
}

void terminal_on_key(char c) {
    if (c == 17) {
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

    if (c == 18) {
        if (hist_idx != -1) {
            hist_idx++;
            input_clear_line();
            if (hist_idx < hist_count) {
                kstrcpy(input_buf, history[hist_idx]);
            } else {
                hist_idx = -1;
                kmemset(input_buf, 0, INPUT_MAX);
            }
            input_len = kstrlen(input_buf);
            input_pos = input_len;
            input_redraw();
        }
        return;
    }

    if (c == '\n') {
        if (fb_available()) fb_cursor_show(false);
        else vga_cursor_show(false);

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