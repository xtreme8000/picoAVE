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

#ifndef STR_BUILDER_H
#define STR_BUILDER_H

#include <stdint.h>

/**
 * Finish the string. Appends a zero character.
 */
static inline void str_finish(char* s) {
	*s = 0;
}

/**
 * Append a single character to the string.
 */
static inline char* str_char(char* s, char c) {
	*(s++) = c;
	return s;
}

/**
 * Append another string to the string.
 */
char* str_append(char* s, char* a);

/**
 * Append an unsigned int to the string.
 */
char* str_uint(char* s, uint32_t i);

#endif