#pragma once
#include <stdint.h>

struct VesaInfo {
    uint32_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
};

void vesa_init(const VesaInfo* info);

bool vesa_available();
void vesa_put_pixel(int x, int y, uint32_t rgb);
void vesa_fill_rect(int x, int y, int w, int h, uint32_t rgb);
void vesa_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);

int vesa_width();
int vesa_height();