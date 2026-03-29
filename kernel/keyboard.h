#pragma once
#include <stdint.h>

void keyboard_init();
void keyboard_handler();

char keyboard_getchar();
bool keyboard_haschar();

void keyboard_set_callback(void (*cb)(char));