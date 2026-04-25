#pragma once
#include "gfx_types.h"

struct Surface {
    uint8_t* pixels;
    int      width;
    int      height;
    int      pitch;
    bool     owned;

    Surface() : pixels(nullptr), width(0), height(0), pitch(0), owned(false) {}
};

Surface surface_from_ptr(uint8_t* ptr, int w, int h, int pitch);

Surface surface_create(int w, int h);

void surface_free(Surface& s);

static inline uint32_t* surface_row(const Surface& s, int y) {
    return (uint32_t*)(s.pixels + y * s.pitch);
}

static inline Color32 surface_get(const Surface& s, int x, int y) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return 0;
    return surface_row(s, y)[x];
}

static inline void surface_set(const Surface& s, int x, int y, Color32 c) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return;
    surface_row(s, y)[x] = c;
}

void surface_blit(Surface& dst, int dx, int dy, const Surface& src);

void surface_blit_rect(Surface& dst, int dx, int dy,
                       const Surface& src, Rect src_rect);

void surface_blit_alpha(Surface& dst, int dx, int dy, const Surface& src);

void surface_scale(Surface& dst, const Surface& src);