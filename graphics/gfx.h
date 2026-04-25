#pragma once
#include "surface.h"
#include "painter.h"

void gfx_init();

bool gfx_ready();

Surface& gfx_screen();

Surface& gfx_backbuffer();

void gfx_flip();
void gfx_flip_rect(Rect r);

Painter& gfx_painter();

int gfx_width();
int gfx_height();