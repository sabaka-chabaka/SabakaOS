#include "gfx.h"
#include "../kernel/fb.h"

static Surface s_screen;
static Surface s_back;
static Painter s_painter;
static bool    s_ready = false;

void gfx_init() {
    if (!fb_available()) return;

    s_screen = surface_from_ptr(
        (uint8_t*)fb_phys_addr(),
        (int)fb_width(),
        (int)fb_height(),
        (int)fb_width() * 4
    );

    s_back = surface_create((int)fb_width(), (int)fb_height());
    if (!s_back.pixels) {
        s_back = s_screen;
        s_back.owned = false;
    }

    s_painter.begin(s_back);

    s_ready = true;
}

bool     gfx_ready()      { return s_ready; }
Surface& gfx_screen()     { return s_screen; }
Surface& gfx_backbuffer() { return s_back; }
Painter& gfx_painter()    { s_painter.begin(s_back); return s_painter; }
int      gfx_width()      { return s_screen.width; }
int      gfx_height()     { return s_screen.height; }

void gfx_flip() {
    if (!s_ready) return;
    if (s_back.pixels == s_screen.pixels) return;
    surface_blit(s_screen, 0, 0, s_back);
}

void gfx_flip_rect(Rect r) {
    if (!s_ready) return;
    if (s_back.pixels == s_screen.pixels) return;
    surface_blit_rect(s_screen, r.x, r.y, s_back, r);
}