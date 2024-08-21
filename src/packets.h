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

#ifndef PACKETS_H
#define PACKETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PACKET_HEADER_LENGTH 4
#define PACKET_BODY_LENGTH 32
#define PACKET_SUBLANE_LENGTH 8
#define PACKET_SUBLANE_COUNT (PACKET_BODY_LENGTH / PACKET_SUBLANE_LENGTH)

struct packet {
	uint8_t header[PACKET_HEADER_LENGTH];
	uint8_t data[PACKET_BODY_LENGTH];
};

void packets_init(void);
void packets_encode(struct packet* p, size_t amount, bool hsync, bool vsync,
					uint32_t* tmds0, uint32_t* tmds1, uint32_t* tmds2);
void packets_encode_audio(uint32_t samples[4], size_t frame, bool hsync,
						  bool vsync, uint32_t* tmds0, uint32_t* tmds1,
						  uint32_t* tmds2);
size_t packets_length(size_t amount);

#endif