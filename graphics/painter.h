#pragma once
#include "surface.h"

struct Painter {
    Surface* target;
    Rect     clip;
    Color32  color;
    Color32  bg;
    bool     transparent_bg;

    void begin(Surface& s);

    void set_clip(Rect r);
    void reset_clip();

    void put_pixel   (int x, int y);
    void draw_hline  (int x, int y, int len);
    void draw_vline  (int x, int y, int len);
    void draw_line   (int x0, int y0, int x1, int y1);
    void draw_rect   (Rect r);
    void fill_rect   (Rect r);
    void fill_rect   (int x, int y, int w, int h);
    void draw_rounded_rect(Rect r, int radius);
    void fill_rounded_rect(Rect r, int radius);
    void draw_circle (int cx, int cy, int radius);
    void fill_circle (int cx, int cy, int radius);
    void draw_triangle(int x0,int y0, int x1,int y1, int x2,int y2);
    void fill_triangle(int x0,int y0, int x1,int y1, int x2,int y2);

    void fill_rect_gradient_v(Rect r, Color32 top, Color32 bottom);
    void fill_rect_gradient_h(Rect r, Color32 left, Color32 right);

    void draw_char (int x, int y, char c);
    void draw_text (int x, int y, const char* s);
    void draw_text_aligned(Rect r, const char* s, Align align);

    void blit       (int x, int y, const Surface& src);
    void blit_rect  (int x, int y, const Surface& src, Rect src_rect);
    void blit_alpha (int x, int y, const Surface& src);

private:
    inline void plot(int x, int y) {
        if (x < clip.x || y < clip.y ||
            x >= clip.right() || y >= clip.bottom()) return;
        surface_set(*target, x, y, color);
    }
    inline void plot_blend(int x, int y) {
        if (x < clip.x || y < clip.y ||
            x >= clip.right() || y >= clip.bottom()) return;
        Color32 dst = surface_get(*target, x, y);
        surface_set(*target, x, y, color_blend(color, dst));
    }
};
