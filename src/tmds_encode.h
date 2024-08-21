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

#ifndef TMDS_ENCODE_H
#define TMDS_ENCODE_H

#include <stdbool.h>
#include <stdint.h>

#define DATA_ISLAND_PREAMBLE_LENGTH 8
#define DATA_ISLAND_GUARD_BAND 0x4CD33

#define VIDEO_DATA_PREAMBLE_LENGTH 8
#define VIDEO_DATA_GUARD_BAND_0 0xb32cc
#define VIDEO_DATA_GUARD_BAND_1 0x4cd33
#define VIDEO_DATA_GUARD_BAND_2 0xb32cc

extern uint16_t tmds_terc4_symbols[16];
extern uint32_t tmds_sync_symbols[4];
extern uint32_t tmds_symbols_cbcr[9];

void tmds_encode_init(void);
void tmds_encode_setup(void);
uint32_t tmds_encode_y1y2(const uint32_t* pixbuf, size_t length,
						  uint32_t* tmds);
uint32_t tmds_encode_cbcr(const uint32_t* pixbuf, size_t length,
						  uint32_t* tmds);
uint32_t tmds_sync_lookup(bool vsync, bool hsync);
void tmds_encode_sync(size_t length, bool vsync, uint32_t* tmds0,
					  uint32_t* tmds1, uint32_t* tmds2);
void tmds_encode_sync_video(uint32_t* tmds0, uint32_t* tmds1, uint32_t* tmds2);

#endif