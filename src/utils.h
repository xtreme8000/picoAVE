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

#ifndef UTILS_H
#define UTILS_H

#include "pico/stdlib.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define READ_BIT(x, i) (((x) & (1 << (i))) ? 1 : 0)

#define CORE0_DATA __scratch_x("core0_data")
#define CORE0_CODE __scratch_x("core0_code") __attribute__((optimize("Os")))

#define CORE1_DATA __scratch_y("core1_data")
#define CORE1_CODE __scratch_y("core1_code") __attribute__((optimize("Os")))

static inline uint32_t next_pow2(uint32_t x) {
	return (x == 1) ? 1 : 1 << (32 - __builtin_clz(x - 1));
}

static inline size_t min_n(size_t a, size_t b) {
	return a < b ? a : b;
}

static inline size_t max_n(size_t a, size_t b) {
	return a > b ? a : b;
}

static inline size_t clamp_n(size_t x, size_t min, size_t max) {
	return min_n(max_n(x, min), max);
}

#endif