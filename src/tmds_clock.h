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

#ifndef TMDS_CLOCK_H
#define TMDS_CLOCK_H

#include "pico/platform.h"
#include "tmds_serializer.h"

struct tmds_clock {
	uint pio_sm;
};

void tmds_clock_init(void);
void tmds_clock_create(struct tmds_clock* s, uint gpio_base);
void tmds_start_with_clock(struct tmds_clock* c, struct tmds_serializer* s,
						   size_t length);

#endif