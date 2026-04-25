#pragma once
#include <stdint.h>

typedef uint32_t Color32;

static inline Color32 color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline Color32 color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint8_t color_r(Color32 c) { return (c >> 16) & 0xFF; }
static inline uint8_t color_g(Color32 c) { return (c >>  8) & 0xFF; }
static inline uint8_t color_b(Color32 c) { return  c        & 0xFF; }
static inline uint8_t color_a(Color32 c) { return (c >> 24) & 0xFF; }

static inline Color32 color_blend(Color32 src, Color32 dst) {
    uint32_t a = color_a(src);
    if (a == 0xFF) return src;
    if (a == 0x00) return dst;
    uint32_t ia = 255 - a;
    uint8_t r = (uint8_t)((color_r(src) * a + color_r(dst) * ia) / 255);
    uint8_t g = (uint8_t)((color_g(src) * a + color_g(dst) * ia) / 255);
    uint8_t b = (uint8_t)((color_b(src) * a + color_b(dst) * ia) / 255);
    return color_rgb(r, g, b);
}

namespace Palette {
    static const Color32 Black      = 0xFF0D0D0D;
    static const Color32 White      = 0xFFF2F2F2;
    static const Color32 Red        = 0xFFFF5555;
    static const Color32 Green      = 0xFF50FA7B;
    static const Color32 Yellow     = 0xFFF1FA8C;
    static const Color32 Blue       = 0xFFBD93F9;
    static const Color32 Cyan       = 0xFF8BE9FD;
    static const Color32 Magenta    = 0xFFFF79C6;
    static const Color32 Orange     = 0xFFFFB86C;
    static const Color32 Gray       = 0xFF44475A;
    static const Color32 DarkGray   = 0xFF282A36;
    static const Color32 Transparent= 0x00000000;
}

struct Point {
    int x, y;
    Point() : x(0), y(0) {}
    Point(int x, int y) : x(x), y(y) {}
    Point operator+(const Point& o) const { return {x+o.x, y+o.y}; }
    Point operator-(const Point& o) const { return {x-o.x, y-o.y}; }
    bool operator==(const Point& o) const { return x==o.x && y==o.y; }
};

struct Size {
    int w, h;
    Size() : w(0), h(0) {}
    Size(int w, int h) : w(w), h(h) {}
    bool empty() const { return w <= 0 || h <= 0; }
};

struct Rect {
    int x, y, w, h;

    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
    Rect(Point pos, Size sz) : x(pos.x), y(pos.y), w(sz.w), h(sz.h) {}

    int  right()  const { return x + w; }
    int  bottom() const { return y + h; }
    bool empty()  const { return w <= 0 || h <= 0; }

    bool contains(int px, int py) const {
        return px >= x && px < x+w && py >= y && py < y+h;
    }
    bool contains(Point p) const { return contains(p.x, p.y); }

    Rect intersect(const Rect& o) const {
        int lx = x > o.x ? x : o.x;
        int ly = y > o.y ? y : o.y;
        int rx = right()  < o.right()  ? right()  : o.right();
        int ry = bottom() < o.bottom() ? bottom() : o.bottom();
        if (rx <= lx || ry <= ly) return Rect(0,0,0,0);
        return Rect(lx, ly, rx-lx, ry-ly);
    }

    Rect unite(const Rect& o) const {
        if (empty()) return o;
        if (o.empty()) return *this;
        int lx = x < o.x ? x : o.x;
        int ly = y < o.y ? y : o.y;
        int rx = right()  > o.right()  ? right()  : o.right();
        int ry = bottom() > o.bottom() ? bottom() : o.bottom();
        return Rect(lx, ly, rx-lx, ry-ly);
    }

    Rect translated(int dx, int dy) const { return Rect(x+dx, y+dy, w, h); }
    Rect translated(Point d)        const { return translated(d.x, d.y); }

    Point top_left()     const { return {x,   y};   }
    Point top_right()    const { return {x+w, y};   }
    Point bottom_left()  const { return {x,   y+h}; }
    Point bottom_right() const { return {x+w, y+h}; }
    Point center()       const { return {x+w/2, y+h/2}; }
    Size  size()         const { return {w, h}; }
};

enum class Align {
    Left,
    Center,
    Right,
};