#include "terminal.h"
#include "kstring.h"
#include "fb.h"

static unsigned short* const VGA = (unsigned short*)0xB8000;
static const int VGA_COLS         = 80;
static const int VGA_TERM_START   = 3;
static const int VGA_TERM_END     = 24;

static uint32_t cur_fg = Color::White;
static uint32_t cur_bg = Color::Black;

static int vga_row = VGA_TERM_START;
static int vga_col = 0;

static int fb_term_x = 0;
static int fb_term_y = 0;
static int fb_term_w = 0;
static int fb_term_h = 0;
static int fb_cx = 0;
static int fb_cy = 0;

static void (*execute_cb)(const char*) = nullptr;
void terminal_set_execute_cb(void (*cb)(const char*)) { execute_cb = cb; }

static inline unsigned short vga_entry(char c, uint8_t fg, uint8_t bg) {
    return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
}

static uint8_t rgb_to_vga(uint32_t c) {
    if (c == Color::White)      return 15;
    if (c == Color::Green)      return 10;
    if (c == Color::Yellow)     return 14;
    if (c == Color::Cyan)       return 11;
    if (c == Color::Red)        return 12;
    if (c == Color::Magenta)    return 13;
    if (c == Color::Blue)       return 9;
    if (c == Color::Orange)     return 14;
    return 7;
}
static void vga_scroll() {
    for (int r = VGA_TERM_START; r < VGA_TERM_END-1; r++)
        for (int c = 0; c < VGA_COLS; c++)
            VGA[r*VGA_COLS+c] = VGA[(r+1)*VGA_COLS+c];
    for (int c = 0; c < VGA_COLS; c++)
        VGA[(VGA_TERM_END-1)*VGA_COLS+c] = vga_entry(' ', 15, 0);
}
static void vga_show_cursor(bool on) {
    if (vga_row < VGA_TERM_END)
        VGA[vga_row*VGA_COLS+vga_col] = vga_entry(on ? '_' : ' ', 14, 0);
}

static void fb_show_cursor(bool on) {
    int px = fb_term_x + fb_cx;
    int py = fb_term_y + fb_cy + FB_FONT_H - 3;
    fb_fill_rect(px, py, FB_FONT_W, 3, on ? Color::Cursor : Color::Black);
}

static void fb_newline() {
    fb_cx = 0;
    fb_cy += FB_FONT_H;
    if (fb_cy + FB_FONT_H > fb_term_h) {
        fb_scroll_region(fb_term_x, fb_term_y,
                         fb_term_w, fb_term_h, 1, Color::Black);
        fb_cy = fb_term_h - FB_FONT_H;
    }
}

void terminal_putchar(char c) {
    if (fb_available()) {
        fb_show_cursor(false);
        if (c == '\n') {
            fb_newline();
        } else if (c == '\t') {
            int next = ((fb_cx / FB_FONT_W / 4) + 1) * 4 * FB_FONT_W;
            while (fb_cx < next && fb_cx + FB_FONT_W <= fb_term_w) {
                fb_draw_char(fb_term_x + fb_cx, fb_term_y + fb_cy,
                             ' ', cur_fg, cur_bg);
                fb_cx += FB_FONT_W;
            }
        } else {
            fb_draw_char(fb_term_x + fb_cx, fb_term_y + fb_cy,
                         c, cur_fg, cur_bg);
            fb_cx += FB_FONT_W;
            if (fb_cx + FB_FONT_W > fb_term_w) fb_newline();
        }
        fb_show_cursor(true);
    } else {
        vga_show_cursor(false);
        uint8_t fg = rgb_to_vga(cur_fg);
        if (c == '\n') {
            vga_col = 0; vga_row++;
            if (vga_row >= VGA_TERM_END) { vga_scroll(); vga_row = VGA_TERM_END-1; }
        } else if (c == '\t') {
            int tab = ((vga_col/4)+1)*4;
            while (vga_col < tab && vga_col < VGA_COLS-1) {
                VGA[vga_row*VGA_COLS+vga_col] = vga_entry(' ', fg, 0);
                vga_col++;
            }
        } else {
            VGA[vga_row*VGA_COLS+vga_col] = vga_entry(c, fg, 0);
            if (++vga_col >= VGA_COLS) {
                vga_col = 0; vga_row++;
                if (vga_row >= VGA_TERM_END) { vga_scroll(); vga_row = VGA_TERM_END-1; }
            }
        }
        vga_show_cursor(true);
    }
}

void terminal_puts(const char* s)         { while(*s) terminal_putchar(*s++); }
void terminal_put_uint(uint32_t n, int b) { char buf[34]; kuitoa(n,buf,b); terminal_puts(buf); }
void terminal_put_int(int32_t n)          { char buf[12]; kitoa(n,buf,10); terminal_puts(buf); }
void terminal_newline()                   { terminal_putchar('\n'); }

void terminal_set_color_fg(uint8_t idx) {
    static const uint32_t vga_pal[16] = {
        Color::Black, 0x0000AA, Color::Green,   Color::Cyan,
        Color::Red,   Color::Magenta, 0xAA5500, 0xAAAAAA,
        0x555555,     Color::Blue,    Color::Green, Color::Cyan,
        Color::Red,   Color::Magenta, Color::Yellow, Color::White
    };
    cur_fg = (idx < 16) ? vga_pal[idx] : Color::White;
}
void terminal_reset_color() { cur_fg = Color::White; cur_bg = Color::Black; }

void terminal_clear() {
    if (fb_available()) {
        fb_fill_rect(fb_term_x, fb_term_y, fb_term_w, fb_term_h, Color::Black);
        fb_cx = 0; fb_cy = 0;
    } else {
        for (int r = VGA_TERM_START; r < VGA_TERM_END; r++)
            for (int c = 0; c < VGA_COLS; c++)
                VGA[r*VGA_COLS+c] = vga_entry(' ', 15, 0);
        vga_row = VGA_TERM_START; vga_col = 0;
    }
}

static const int INPUT_MAX = 256;
static const int HIST_MAX  = 16;

static char input_buf[INPUT_MAX];
static int  input_len = 0, input_pos = 0;
static char history[HIST_MAX][INPUT_MAX];
static int  hist_count = 0, hist_idx = -1;

static int  vga_prompt_row = VGA_TERM_START, vga_prompt_col = 0;
static int  fb_prompt_cx   = 0,              fb_prompt_cy   = 0;

static char prompt_path[256] = "/";
void terminal_set_prompt_path(const char* path) {
    int i = 0;
    while (path[i] && i < 255) { prompt_path[i] = path[i]; i++; }
    prompt_path[i] = 0;
}

static void print_prompt() {
    if (fb_available()) fb_show_cursor(false);
    else vga_show_cursor(false);

    cur_fg = Color::Prompt;     terminal_puts("sabaka");
    cur_fg = Color::PromptAt;   terminal_putchar('@');
    cur_fg = Color::PromptHost; terminal_puts("SabakaOS");
    cur_fg = Color::White;      terminal_puts(": ");
    cur_fg = Color::PromptPath; terminal_puts("~");
    if (prompt_path[0] == '/' && prompt_path[1] != 0) terminal_puts(prompt_path);
    cur_fg = Color::PromptArrow; terminal_puts(" > ");
    cur_fg = Color::Input;
}

static void input_redraw_fb() {
    fb_show_cursor(false);
    int px = fb_term_x + fb_prompt_cx;
    int py = fb_term_y + fb_prompt_cy;
    int end_x = fb_term_x + fb_term_w;

    for (int i = 0; i < input_len; i++) {
        fb_draw_char(px, py, input_buf[i], Color::Input, Color::Black);
        px += FB_FONT_W;
        if (px + FB_FONT_W > end_x) { px = fb_term_x; py += FB_FONT_H; }
    }

    int erase = px;
    while (erase + FB_FONT_W <= end_x) {
        fb_fill_rect(erase, py, FB_FONT_W, FB_FONT_H, Color::Black);
        erase += FB_FONT_W;
    }

    fb_cx = fb_prompt_cx + input_pos * FB_FONT_W;
    fb_cy = fb_prompt_cy;
    while (fb_cx + FB_FONT_W > fb_term_w) { fb_cx -= fb_term_w; fb_cy += FB_FONT_H; }
    fb_show_cursor(true);
}

static void input_redraw_vga() {
    vga_show_cursor(false);
    int r = vga_prompt_row, c = vga_prompt_col;
    for (int i = 0; i < input_len; i++) {
        VGA[r*VGA_COLS+c] = vga_entry(input_buf[i], 10, 0);
        if (++c >= VGA_COLS) { c=0; r++; }
    }
    for (int fc = c; fc < VGA_COLS; fc++)
        VGA[r*VGA_COLS+fc] = vga_entry(' ', 15, 0);
    vga_row = vga_prompt_row;
    vga_col = vga_prompt_col + input_pos;
    while (vga_col >= VGA_COLS) { vga_col -= VGA_COLS; vga_row++; }
    vga_show_cursor(true);
}

static void input_redraw() {
    if (fb_available()) input_redraw_fb(); else input_redraw_vga();
}

static void input_clear_line() {
    if (fb_available()) {
        int px = fb_term_x + fb_prompt_cx, py = fb_term_y + fb_prompt_cy;
        for (int i = 0; i < input_len; i++) {
            fb_fill_rect(px, py, FB_FONT_W, FB_FONT_H, Color::Black);
            px += FB_FONT_W;
            if (px + FB_FONT_W > fb_term_x + fb_term_w) { px = fb_term_x; py += FB_FONT_H; }
        }
    } else {
        int r = vga_prompt_row, c = vga_prompt_col;
        for (int i = 0; i < input_len; i++) {
            VGA[r*VGA_COLS+c] = vga_entry(' ', 15, 0);
            if (++c >= VGA_COLS) { c=0; r++; }
        }
    }
    input_len = 0; input_pos = 0;
}

static void start_input() {
    input_len = input_pos = 0;
    hist_idx = -1;
    kmemset(input_buf, 0, INPUT_MAX);
    print_prompt();
    if (fb_available()) {
        fb_prompt_cx = fb_cx;
        fb_prompt_cy = fb_cy;
        fb_show_cursor(true);
    } else {
        vga_prompt_row = vga_row;
        vga_prompt_col = vga_col;
        vga_show_cursor(true);
    }
}

void terminal_on_key(char c) {
    if (c == 17) { // UP
        if (hist_count > 0) {
            if (hist_idx == -1) hist_idx = hist_count - 1;
            else if (hist_idx > 0) hist_idx--;
            input_clear_line();
            kstrcpy(input_buf, history[hist_idx]);
            input_len = kstrlen(input_buf); input_pos = input_len;
            input_redraw();
        }
        return;
    }
    if (c == 18) { // DOWN
        if (hist_idx != -1) {
            hist_idx++;
            input_clear_line();
            if (hist_idx < hist_count) kstrcpy(input_buf, history[hist_idx]);
            else { hist_idx = -1; kmemset(input_buf, 0, INPUT_MAX); }
            input_len = kstrlen(input_buf); input_pos = input_len;
            input_redraw();
        }
        return;
    }
    if (c == '\n') {
        if (fb_available()) fb_show_cursor(false); else vga_show_cursor(false);
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
            for (int i=input_pos-1;i<input_len-1;i++) input_buf[i]=input_buf[i+1];
            input_len--; input_pos--;
            input_buf[input_len]=0;
            input_redraw();
        }
    } else if (c >= 32 && c < 127) {
        if (input_len < INPUT_MAX-1) {
            for (int i=input_len;i>input_pos;i--) input_buf[i]=input_buf[i-1];
            input_buf[input_pos++]=c; input_len++;
            input_buf[input_len]=0;
            input_redraw();
        }
    }
}

void terminal_init() {
    if (fb_available()) {
        fb_term_x = 0;
        fb_term_y = 28;
        fb_term_w = (int)fb_width();
        fb_term_h = (int)fb_height() - 28;
    }
    terminal_clear();
}

void terminal_reply_input() {
    start_input();
}