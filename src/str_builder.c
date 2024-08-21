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

#include "str_builder.h"

char* str_append(char* s, char* a) {
	while(*a)
		*(s++) = *(a++);

	return s;
}

char* str_uint(char* s, uint32_t i) {
	char buf[10];
	char* last = buf;

	while(1) {
		*(last++) = (i % 10) + '0';
		i /= 10;

		if(!i)
			break;
	}

	while(buf != last)
		*(s++) = *(--last);

	return s;
}