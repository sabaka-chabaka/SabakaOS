#include "surface.h"
#include "../kernel/heap.h"
#include "../kernel/kstring.h"
#include <stddef.h>

Surface surface_from_ptr(uint8_t* ptr, int w, int h, int pitch) {
    Surface s;
    s.pixels = ptr;
    s.width  = w;
    s.height = h;
    s.pitch  = pitch;
    s.owned  = false;
    return s;
}

Surface surface_create(int w, int h) {
    Surface s;
    s.width  = w;
    s.height = h;
    s.pitch  = w * 4;
    s.owned  = true;
    s.pixels = (uint8_t*)kmalloc((uint32_t)(w * h * 4));
    if (s.pixels) kmemset(s.pixels, 0, (uint32_t)(w * h * 4));
    return s;
}

void surface_free(Surface& s) {
    if (s.owned && s.pixels) {
        kfree(s.pixels);
        s.pixels = nullptr;
    }
    s.owned = false;
}

void surface_blit(Surface& dst, int dx, int dy, const Surface& src) {
    surface_blit_rect(dst, dx, dy, src, Rect(0, 0, src.width, src.height));
}

void surface_blit_rect(Surface& dst, int dx, int dy,
                       const Surface& src, Rect sr)
{
    if (sr.x < 0) { dx -= sr.x; sr.w += sr.x; sr.x = 0; }
    if (sr.y < 0) { dy -= sr.y; sr.h += sr.y; sr.y = 0; }
    if (sr.right()  > src.width)  sr.w = src.width  - sr.x;
    if (sr.bottom() > src.height) sr.h = src.height - sr.y;

    if (dx < 0) { sr.x -= dx; sr.w += dx; dx = 0; }
    if (dy < 0) { sr.y -= dy; sr.h += dy; dy = 0; }
    if (dx + sr.w > dst.width)  sr.w = dst.width  - dx;
    if (dy + sr.h > dst.height) sr.h = dst.height - dy;

    if (sr.w <= 0 || sr.h <= 0) return;

    int row_bytes = sr.w * 4;
    for (int row = 0; row < sr.h; row++) {
        uint8_t* s_row = src.pixels + (sr.y + row) * src.pitch + sr.x * 4;
        uint8_t* d_row = dst.pixels + (dy  + row) * dst.pitch + dx   * 4;
        kmemcpy(d_row, s_row, (size_t)row_bytes);
    }
}

void surface_blit_alpha(Surface& dst, int dx, int dy, const Surface& src) {
    int sx = 0, sy = 0;
    int w = src.width, h = src.height;

    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > dst.width)  w = dst.width  - dx;
    if (dy + h > dst.height) h = dst.height - dy;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint32_t* s_row = (uint32_t*)(src.pixels + (sy+row)*src.pitch) + sx;
        uint32_t* d_row = (uint32_t*)(dst.pixels + (dy+row)*dst.pitch) + dx;
        for (int col = 0; col < w; col++) {
            d_row[col] = color_blend(s_row[col], d_row[col]);
        }
    }
}

void surface_scale(Surface& dst, const Surface& src) {
    if (src.width == 0 || src.height == 0) return;
    for (int dy = 0; dy < dst.height; dy++) {
        int sy = dy * src.height / dst.height;
        uint32_t* d_row = surface_row(dst, dy);
        uint32_t* s_row = surface_row(src, sy);
        for (int dx2 = 0; dx2 < dst.width; dx2++) {
            int sx = dx2 * src.width / dst.width;
            d_row[dx2] = s_row[sx];
        }
    }
}