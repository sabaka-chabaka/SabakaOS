#pragma once
#include "../graphics/gfx.h"

void graphics_init();
bool graphics_ready();

struct Window {
    Rect    bounds;
    char    title[64];
    bool    visible;
    bool    focused;
    Surface backbuf;
    Painter painter;
};

int  win_create (int x, int y, int w, int h, const char* title);
void win_destroy(int id);
void win_show   (int id);
void win_hide   (int id);
void win_move   (int id, int x, int y);
void win_focus  (int id);

Painter& win_painter(int id);

void win_flush  (int id);

void win_flush_all();

static const int TITLEBAR_H  = 28;
static const int BORDER_W    = 1;
static const int MAX_WINDOWS = 16;

void cursor_draw(int x, int y);
void cursor_hide();

namespace WinColor {
    static const Color32 Desktop          = 0xFF0D1117;
    static const Color32 TitlebarActive   = 0xFF2D2D3F;
    static const Color32 TitlebarInactive = 0xFF1A1A28;
    static const Color32 TitleText        = 0xFFCDD6F4;
    static const Color32 TitleTextDim     = 0xFF6C7086;
    static const Color32 Border           = 0xFF44475A;
    static const Color32 BorderFocused    = 0xFFBD93F9;
    static const Color32 ClientBg         = 0xFF1E1E2E;
    static const Color32 BtnClose         = 0xFFFF5F57;
    static const Color32 BtnMin           = 0xFFFFBD2E;
    static const Color32 BtnMax           = 0xFF28C840;
}