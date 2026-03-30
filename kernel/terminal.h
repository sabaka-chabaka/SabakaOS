#pragma once
#include <stdint.h>

void terminal_init();
void terminal_set_execute_cb(void (*cb)(const char*));

void terminal_putchar(char c);
void terminal_puts(const char* s);
void terminal_put_uint(uint32_t n, int base = 10);
void terminal_put_int(int32_t n);
void terminal_newline();
void terminal_clear();
void terminal_set_color_fg(uint8_t fg);
void terminal_reset_color();
void terminal_on_key(char c);
void terminal_reply_input();