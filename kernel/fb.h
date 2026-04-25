#pragma once
#include <stdint.h>

struct MultibootInfo {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint8_t  syms[16];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} __attribute__((packed));

#define VBE_DISPI_IOPORT_INDEX  0x01CE
#define VBE_DISPI_IOPORT_DATA   0x01CF

#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

#define VBE_DISPI_DISABLED      0x00
#define VBE_DISPI_ENABLED       0x01
#define VBE_DISPI_LFB_ENABLED   0x40

#define BGA_LFB_PHYS_ADDR       0xE0000000

bool fb_init(const MultibootInfo* mbi);
bool fb_available();

uint32_t fb_width();
uint32_t fb_height();
uint8_t  fb_bpp();

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void fb_put_pixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_hline(int x, int y, int len, uint32_t color);
void fb_draw_vline(int x, int y, int len, uint32_t color);
void fb_draw_rect(int x, int y, int w, int h, uint32_t color);

static const int FB_FONT_W = 8;
static const int FB_FONT_H = 16;

void fb_draw_char(int x, int y, char c,        uint32_t fg, uint32_t bg);
void fb_draw_str (int x, int y, const char* s, uint32_t fg, uint32_t bg);
void fb_draw_str_transparent(int x, int y, const char* s, uint32_t fg);

void fb_scroll_region(int rx, int ry, int rw, int rh, int lines, uint32_t fill);

namespace Color {
    static const uint32_t Black       = 0x0D0D0D;
    static const uint32_t White       = 0xF2F2F2;
    static const uint32_t Red         = 0xFF5555;
    static const uint32_t Green       = 0x50FA7B;
    static const uint32_t Yellow      = 0xF1FA8C;
    static const uint32_t Blue        = 0xBD93F9;
    static const uint32_t Cyan        = 0x8BE9FD;
    static const uint32_t Magenta     = 0xFF79C6;
    static const uint32_t Orange      = 0xFFB86C;
    static const uint32_t Gray        = 0x44475A;
    static const uint32_t DarkGray    = 0x282A36;
    static const uint32_t HeaderBg    = 0x1E1E2E;
    static const uint32_t TermBg      = 0x0D0D0D;
    static const uint32_t Prompt      = 0x8BE9FD;
    static const uint32_t PromptAt    = 0xF2F2F2;
    static const uint32_t PromptHost  = 0x50FA7B;
    static const uint32_t PromptPath  = 0x8BE9FD;
    static const uint32_t PromptArrow = 0xBD93F9;
    static const uint32_t Input       = 0xF2F2F2;
    static const uint32_t Cursor      = 0xF1FA8C;
}