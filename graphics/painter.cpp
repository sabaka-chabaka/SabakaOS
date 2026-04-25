#include "painter.h"
#include "../kernel/kstring.h"

void Painter::begin(Surface& s) {
    target = &s;
    clip   = Rect(0, 0, s.width, s.height);
    color  = Palette::White;
    bg     = Palette::Black;
    transparent_bg = false;
}

void Painter::set_clip(Rect r) {
    clip = clip.intersect(r);
}

void Painter::reset_clip() {
    if (target) clip = Rect(0, 0, target->width, target->height);
}

void Painter::put_pixel(int x, int y) {
    plot(x, y);
}

void Painter::draw_hline(int x, int y, int len) {
    if (len <= 0) return;
    if (y < clip.y || y >= clip.bottom()) return;
    int x0 = x < clip.x ? clip.x : x;
    int x1 = x+len > clip.right() ? clip.right() : x+len;
    if (x0 >= x1) return;
    uint32_t* row = surface_row(*target, y);
    for (int i = x0; i < x1; i++) row[i] = color;
}

void Painter::draw_vline(int x, int y, int len) {
    if (len <= 0) return;
    if (x < clip.x || x >= clip.right()) return;
    int y0 = y < clip.y ? clip.y : y;
    int y1 = y+len > clip.bottom() ? clip.bottom() : y+len;
    for (int i = y0; i < y1; i++) surface_row(*target, i)[x] = color;
}

void Painter::draw_line(int x0, int y0, int x1, int y1) {
    int dx =  (x1>x0 ? x1-x0 : x0-x1);
    int dy = -(y1>y0 ? y1-y0 : y0-y1);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        plot(x0, y0);
        if (x0==x1 && y0==y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Painter::draw_rect(Rect r) {
    draw_hline(r.x, r.y,       r.w);
    draw_hline(r.x, r.bottom()-1, r.w);
    draw_vline(r.x, r.y,       r.h);
    draw_vline(r.right()-1, r.y, r.h);
}

void Painter::fill_rect(int x, int y, int w, int h) {
    fill_rect(Rect(x, y, w, h));
}

void Painter::fill_rect(Rect r) {
    Rect cr = clip.intersect(r);
    if (cr.empty()) return;
    for (int row = cr.y; row < cr.bottom(); row++) {
        uint32_t* p = surface_row(*target, row) + cr.x;
        for (int col = 0; col < cr.w; col++) p[col] = color;
    }
}

static void plot_circle_points(Painter& p, int cx, int cy,
                               int x, int y, bool fill) {
    if (fill) {
        p.draw_hline(cx-x, cy-y, 2*x+1);
        if (y != 0) p.draw_hline(cx-x, cy+y, 2*x+1);
    } else {
        p.put_pixel(cx+x, cy+y); p.put_pixel(cx-x, cy+y);
        p.put_pixel(cx+x, cy-y); p.put_pixel(cx-x, cy-y);
        p.put_pixel(cx+y, cy+x); p.put_pixel(cx-y, cy+x);
        p.put_pixel(cx+y, cy-x); p.put_pixel(cx-y, cy-x);
    }
}

void Painter::draw_rounded_rect(Rect r, int rad) {
    if (rad <= 0) { draw_rect(r); return; }
    int cx0 = r.x + rad,     cy0 = r.y + rad;
    int cx1 = r.right()-1-rad, cy1 = r.bottom()-1-rad;
    draw_hline(r.x+rad, r.y,          r.w-2*rad);
    draw_hline(r.x+rad, r.bottom()-1, r.w-2*rad);
    draw_vline(r.x,          r.y+rad, r.h-2*rad);
    draw_vline(r.right()-1,  r.y+rad, r.h-2*rad);
    auto corner = [&](int cx, int cy) {
        int x = 0, y = rad, d = 1 - rad;
        while (x <= y) {
            put_pixel(cx+x, cy-y); put_pixel(cx-x, cy-y);
            put_pixel(cx+x, cy+y); put_pixel(cx-x, cy+y);
            put_pixel(cx+y, cy-x); put_pixel(cx-y, cy-x);
            put_pixel(cx+y, cy+x); put_pixel(cx-y, cy+x);
            if (d < 0) d += 2*x+3; else { d += 2*(x-y)+5; y--; }
            x++;
        }
    };
    corner(cx0, cy0); corner(cx1, cy0);
    corner(cx0, cy1); corner(cx1, cy1);
}

void Painter::fill_rounded_rect(Rect r, int rad) {
    if (rad <= 0) { fill_rect(r); return; }
    fill_rect(Rect(r.x, r.y+rad, r.w, r.h-2*rad));
    fill_rect(Rect(r.x+rad, r.y,          r.w-2*rad, rad));
    fill_rect(Rect(r.x+rad, r.bottom()-rad, r.w-2*rad, rad));

    int cx0 = r.x+rad, cy0 = r.y+rad;
    int cx1 = r.right()-1-rad, cy1 = r.bottom()-1-rad;

    auto fill_corner = [&](int cx, int cy, int qx, int qy) {
        int x = 0, y = rad, d = 1 - rad;
        while (x <= y) {
            draw_hline(qx < 0 ? cx-y : cx, cy + qy*x, y);
            draw_hline(qx < 0 ? cx-x : cx, cy + qy*y, x);
            if (d < 0) d += 2*x+3; else { d += 2*(x-y)+5; y--; }
            x++;
        }
    };
    {int x=0,y=rad,d=1-rad; while(x<=y){draw_hline(cx0-y,cy0-x,y);draw_hline(cx0-x,cy0-y,x);if(d<0)d+=2*x+3;else{d+=2*(x-y)+5;y--;}x++;}}
    {int x=0,y=rad,d=1-rad; while(x<=y){draw_hline(cx1,cy0-x,y);draw_hline(cx1,cy0-y,x);if(d<0)d+=2*x+3;else{d+=2*(x-y)+5;y--;}x++;}}
    {int x=0,y=rad,d=1-rad; while(x<=y){draw_hline(cx0-y,cy1+x,y);draw_hline(cx0-x,cy1+y,x);if(d<0)d+=2*x+3;else{d+=2*(x-y)+5;y--;}x++;}}
    {int x=0,y=rad,d=1-rad; while(x<=y){draw_hline(cx1,cy1+x,y);draw_hline(cx1,cy1+y,x);if(d<0)d+=2*x+3;else{d+=2*(x-y)+5;y--;}x++;}}
    (void)fill_corner;
}

void Painter::draw_circle(int cx, int cy, int r) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        put_pixel(cx+x,cy+y); put_pixel(cx-x,cy+y);
        put_pixel(cx+x,cy-y); put_pixel(cx-x,cy-y);
        put_pixel(cx+y,cy+x); put_pixel(cx-y,cy+x);
        put_pixel(cx+y,cy-x); put_pixel(cx-y,cy-x);
        if (d < 0) d += 2*x+3; else { d += 2*(x-y)+5; y--; }
        x++;
    }
}

void Painter::fill_circle(int cx, int cy, int r) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        draw_hline(cx-y, cy-x, 2*y+1);
        draw_hline(cx-y, cy+x, 2*y+1);
        draw_hline(cx-x, cy-y, 2*x+1);
        draw_hline(cx-x, cy+y, 2*x+1);
        if (d < 0) d += 2*x+3; else { d += 2*(x-y)+5; y--; }
        x++;
    }
}

void Painter::draw_triangle(int x0,int y0, int x1,int y1, int x2,int y2) {
    draw_line(x0,y0, x1,y1);
    draw_line(x1,y1, x2,y2);
    draw_line(x2,y2, x0,y0);
}

void Painter::fill_triangle(int x0,int y0, int x1,int y1, int x2,int y2) {
    if (y0 > y1) { int t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }
    if (y0 > y2) { int t=x0;x0=x2;x2=t; t=y0;y0=y2;y2=t; }
    if (y1 > y2) { int t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; }

    int total_h = y2 - y0;
    if (total_h == 0) return;

    for (int y = y0; y <= y2; y++) {
        bool second_half = (y >= y1);
        int seg_h = second_half ? y2 - y1 : y1 - y0;
        if (seg_h == 0) seg_h = 1;
        float alpha = (float)(y - y0) / total_h;
        float beta  = second_half ? (float)(y - y1) / seg_h
                                  : (float)(y - y0) / seg_h;
        int ax = (int)(x0 + (x2 - x0) * alpha);
        int bx = (int)(second_half ? x1 + (x2 - x1) * beta
                                   : x0 + (x1 - x0) * beta);
        if (ax > bx) { int t=ax; ax=bx; bx=t; }
        draw_hline(ax, y, bx - ax + 1);
    }
}

void Painter::fill_rect_gradient_v(Rect r, Color32 top, Color32 bot) {
    Rect cr = clip.intersect(r);
    if (cr.empty() || r.h == 0) return;
    for (int row = cr.y; row < cr.bottom(); row++) {
        int t = (row - r.y) * 255 / r.h;
        uint8_t rr = (uint8_t)(color_r(top) + (int)(color_r(bot)-color_r(top))*t/255);
        uint8_t gg = (uint8_t)(color_g(top) + (int)(color_g(bot)-color_g(top))*t/255);
        uint8_t bb = (uint8_t)(color_b(top) + (int)(color_b(bot)-color_b(top))*t/255);
        Color32 c = color_rgb(rr, gg, bb);
        uint32_t* p = surface_row(*target, row) + cr.x;
        for (int col = 0; col < cr.w; col++) p[col] = c;
    }
}

void Painter::fill_rect_gradient_h(Rect r, Color32 left, Color32 right) {
    Rect cr = clip.intersect(r);
    if (cr.empty() || r.w == 0) return;
    for (int row = cr.y; row < cr.bottom(); row++) {
        uint32_t* p = surface_row(*target, row);
        for (int col = cr.x; col < cr.right(); col++) {
            int t = (col - r.x) * 255 / r.w;
            uint8_t rr = (uint8_t)(color_r(left) + (int)(color_r(right)-color_r(left))*t/255);
            uint8_t gg = (uint8_t)(color_g(left) + (int)(color_g(right)-color_g(left))*t/255);
            uint8_t bb = (uint8_t)(color_b(left) + (int)(color_b(right)-color_b(left))*t/255);
            p[col] = color_rgb(rr, gg, bb);
        }
    }
}

void Painter::draw_char(int x, int y, char c) {
    const uint8_t* glyph = font_glyph(c);
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            if (bits & (0x80u >> col)) {
                plot(x + col, y + row);
            } else if (!transparent_bg) {
                Color32 saved = color;
                color = bg;
                plot(x + col, y + row);
                color = saved;
            }
        }
    }
}

void Painter::draw_text(int x, int y, const char* s) {
    if (!s) return;
    int cx = x;
    while (*s) {
        if (*s == '\n') { cx = x; y += FONT_H; s++; continue; }
        if (*s == '\t') {
            int tab = ((cx - x) / FONT_W / 4 + 1) * 4 * FONT_W + x;
            while (cx < tab) {
                if (!transparent_bg) {
                    Color32 saved = color; color = bg;
                    draw_char(cx, y, ' ');
                    color = saved;
                }
                cx += FONT_W;
            }
            s++; continue;
        }
        draw_char(cx, y, *s++);
        cx += FONT_W;
    }
}

void Painter::draw_text_aligned(Rect r, const char* s, Align align) {
    if (!s) return;
    int len = 0;
    for (const char* p = s; *p; p++) len++;
    int tw = len * FONT_W;
    int x = r.x;
    int y = r.y + (r.h - FONT_H) / 2;
    if (align == Align::Center) x = r.x + (r.w - tw) / 2;
    else if (align == Align::Right) x = r.right() - tw;
    Rect saved_clip = clip;
    set_clip(r);
    draw_text(x, y, s);
    clip = saved_clip;
}

void Painter::blit(int x, int y, const Surface& src) {
    Surface clipped_dst = *target;
    surface_blit(clipped_dst, x, y, src);
}

void Painter::blit_rect(int x, int y, const Surface& src, Rect sr) {
    surface_blit_rect(*target, x, y, src, sr);
}

void Painter::blit_alpha(int x, int y, const Surface& src) {
    surface_blit_alpha(*target, x, y, src);
}