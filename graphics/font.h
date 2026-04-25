#pragma once
#include <stdint.h>

static const int FONT_W = 8;
static const int FONT_H = 16;

const uint8_t* font_glyph(char c);

int font_str_width(const char* s);

static inline int font_height() { return FONT_H; }