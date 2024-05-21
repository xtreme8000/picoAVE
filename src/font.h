#ifndef FONT_H
#define FONT_H

#include <stddef.h>
#include <stdint.h>

#define FONT_CHAR_WIDTH 8
#define FONT_CHAR_HEIGHT 16

size_t font_encode(const char* str, size_t y, uint32_t* pixbuf);
size_t font_width(const char* str);

#endif