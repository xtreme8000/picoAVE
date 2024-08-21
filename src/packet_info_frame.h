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

#ifndef PACKET_INFO_FRAME
#define PACKET_INFO_FRAME

#include "packets.h"

void packet_avi_info(struct packet* p);
void packet_spd_info(struct packet* p, const char* vendor, const char* product);
void packet_audio_info(struct packet* p, size_t samplerate);
void packet_audio_clk_regen(struct packet* p, uint32_t n, uint32_t cts);

#endif