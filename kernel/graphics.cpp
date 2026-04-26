#include "graphics.h"
#include "kstring.h"

static Window s_windows[MAX_WINDOWS];
static int    s_count = 0;
static bool   s_ready = false;

static Window* get(int id) {
    if (id < 0 || id >= s_count) return nullptr;
    return &s_windows[id];
}

static Rect client_rect(const Window& w) {
    return Rect(
        w.bounds.x + BORDER_W,
        w.bounds.y + TITLEBAR_H,
        w.bounds.w - BORDER_W * 2,
        w.bounds.h - TITLEBAR_H - BORDER_W
    );
}

static void draw_chrome(const Window& w) {
    Painter& p = gfx_painter();

    for (int i = 1; i <= 4; i++) {
        p.color = color_rgba(0, 0, 0, (uint8_t)(40 - i * 8));
        p.draw_rect(Rect(w.bounds.x - i, w.bounds.y - i,
                         w.bounds.w + i*2, w.bounds.h + i*2));
    }

    p.color = w.focused ? WinColor::BorderFocused : WinColor::Border;
    p.draw_rect(w.bounds);

    Rect tbar(w.bounds.x + BORDER_W, w.bounds.y + BORDER_W,
              w.bounds.w - BORDER_W*2, TITLEBAR_H - BORDER_W);

    Color32 tb = w.focused ? WinColor::TitlebarActive : WinColor::TitlebarInactive;
    Color32 tb_top = color_rgba(
        (uint8_t)(color_r(tb) + 15),
        (uint8_t)(color_g(tb) + 15),
        (uint8_t)(color_b(tb) + 15), 255);
    p.fill_rect_gradient_v(tbar, tb_top, tb);

    p.color = w.focused ? WinColor::BorderFocused : WinColor::Border;
    p.draw_hline(w.bounds.x + BORDER_W,
                 w.bounds.y + TITLEBAR_H - 1,
                 w.bounds.w - BORDER_W*2);

    int by = w.bounds.y + TITLEBAR_H / 2;
    int bx = w.bounds.x + 14;
    p.color = WinColor::BtnClose; p.fill_circle(bx,      by, 6);
    p.color = WinColor::BtnMin;   p.fill_circle(bx + 20, by, 6);
    p.color = WinColor::BtnMax;   p.fill_circle(bx + 40, by, 6);

    p.color = w.focused ? WinColor::TitleText : WinColor::TitleTextDim;
    p.transparent_bg = true;
    p.draw_text_aligned(tbar, w.title, Align::Center);
    p.transparent_bg = false;
}

static void blit_client(const Window& w) {
    if (!w.backbuf.pixels) return;
    Rect cr = client_rect(w);
    surface_blit(gfx_backbuffer(), cr.x, cr.y, w.backbuf);
}

bool graphics_ready() { return s_ready; }

void graphics_init() {
    s_count = 0;
    s_ready = true;
}

int win_create(int x, int y, int w, int h, const char* title) {
    if (s_count >= MAX_WINDOWS) return -1;
    int id = s_count++;
    Window& win = s_windows[id];

    win.bounds  = Rect(x, y, w, h);
    win.visible = true;
    win.focused = (id == 0);

    int ti = 0;
    while (title[ti] && ti < 63) { win.title[ti] = title[ti]; ti++; }
    win.title[ti] = 0;

    Rect cr = client_rect(win);
    win.backbuf = surface_create(cr.w, cr.h);
    win.painter.begin(win.backbuf);
    win.painter.color = WinColor::ClientBg;
    win.painter.fill_rect(Rect(0, 0, cr.w, cr.h));

    return id;
}

void win_destroy(int id) {
    Window* w = get(id);
    if (!w) return;
    surface_free(w->backbuf);
    w->visible = false;
}

void win_show(int id)          { Window* w = get(id); if (w) w->visible = true;  }
void win_hide(int id)          { Window* w = get(id); if (w) w->visible = false; }
void win_focus(int id)         { for(int i=0;i<s_count;i++) s_windows[i].focused=(i==id); }
void win_move(int id, int x, int y) {
    Window* w = get(id);
    if (!w) return;
    w->bounds.x = x;
    w->bounds.y = y;
}

Painter& win_painter(int id) {
    Window* w = get(id);
    if (!w) return gfx_painter();
    w->painter.begin(w->backbuf);
    return w->painter;
}

void win_flush(int id) {
    Window* w = get(id);
    if (!w || !w->visible) return;
    draw_chrome(*w);
    blit_client(*w);
    gfx_flip_rect(w->bounds.translated(-5, -5).unite(w->bounds));
}

void win_flush_all() {
    for (int i = 0; i < s_count; i++) {
        if (!s_windows[i].visible) continue;
        draw_chrome(s_windows[i]);
        blit_client(s_windows[i]);
    }
    gfx_flip();
}

static const int CW = 11;
static const int CH = 18;

static const uint8_t CURSOR_SHAPE[CH][CW] = {
    {2,0,0,0,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,2,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0},
    {2,1,1,1,1,1,1,2,0,0,0},
    {2,1,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,1,1,1,2,0},
    {2,1,1,1,1,1,1,1,1,1,2},
    {2,1,1,1,1,1,2,2,2,2,2},
    {2,1,1,2,1,1,2,0,0,0,0},
    {2,1,2,0,2,1,1,2,0,0,0},
    {2,2,0,0,2,1,1,2,0,0,0},
    {2,0,0,0,0,2,1,1,2,0,0},
    {0,0,0,0,0,2,1,1,2,0,0},
    {0,0,0,0,0,0,2,2,0,0,0},
};

static uint32_t s_cursor_bg[CW * CH];
static int      s_cursor_px = -1;
static int      s_cursor_py = -1;

void cursor_hide() {
    if (s_cursor_px < 0) return;
    Surface& scr = gfx_screen();
    for (int row = 0; row < CH; row++) {
        int sy = s_cursor_py + row;
        if (sy < 0 || sy >= scr.height) continue;
        uint32_t* line = surface_row(scr, sy);
        for (int col = 0; col < CW; col++) {
            int sx = s_cursor_px + col;
            if (sx < 0 || sx >= scr.width) continue;
            line[sx] = s_cursor_bg[row * CW + col];
        }
    }
    s_cursor_px = -1;
}

void cursor_draw(int x, int y) {
    cursor_hide();
    Surface& scr = gfx_screen();

    for (int row = 0; row < CH; row++) {
        int sy = y + row;
        for (int col = 0; col < CW; col++) {
            int sx = x + col;
            uint32_t px = 0;
            if (sx >= 0 && sx < scr.width && sy >= 0 && sy < scr.height)
                px = surface_row(scr, sy)[sx];
            s_cursor_bg[row * CW + col] = px;
        }
    }

    for (int row = 0; row < CH; row++) {
        int sy = y + row;
        if (sy < 0 || sy >= scr.height) continue;
        uint32_t* line = surface_row(scr, sy);
        for (int col = 0; col < CW; col++) {
            int sx = x + col;
            if (sx < 0 || sx >= scr.width) continue;
            uint8_t p = CURSOR_SHAPE[row][col];
            if (p == 1) line[sx] = 0xFFFFFFFF;
            else if (p == 2) line[sx] = 0xFF000000;
        }
    }
    s_cursor_px = x;
    s_cursor_py = y;
}