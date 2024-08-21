/*
	Copyright (c) 2024 xtreme8000

	This file is part of picoAVE.

	picoAVE is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	picoAVE is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with picoAVE.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FONT_H
#define FONT_H

#include <stddef.h>
#include <stdint.h>

#define FONT_CHAR_WIDTH 8
#define FONT_CHAR_HEIGHT 16

size_t font_encode(const char* str, size_t y, uint32_t* pixbuf);
size_t font_width(const char* str);

#endif