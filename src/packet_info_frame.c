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

#include <string.h>

#include "packet_info_frame.h"

static void apply_checksum(struct packet* p) {
	uint8_t sum = 0;

	for(size_t k = 0; k < sizeof(p->header) - 1; k++)
		sum += p->header[k];

	for(size_t k = 0; k < sizeof(p->data); k++) {
		if(k % 8 != 7)
			sum += p->data[k];
	}

	p->data[0] = -sum;
}

void packet_avi_info(struct packet* p) {
	memset(p->header, 0, sizeof(p->header));
	memset(p->data, 0, sizeof(p->data));

	p->header[0] = 0x82;
	p->header[1] = 0x02;
	p->header[2] = 13;

	p->data[1] = 0b00100010;
	p->data[2] = 0b01101000;
	p->data[4] = 3;

	apply_checksum(p);
}

// vendor max 8
// product max 16
void packet_spd_info(struct packet* p, const char* vendor,
					 const char* product) {
	memset(p->header, 0, sizeof(p->header));
	memset(p->data, 0, sizeof(p->data));

	p->header[0] = 0x83;
	p->header[1] = 0x01;
	p->header[2] = 25;

	strncpy((char*)p->data + 1, vendor, 8);
	strncpy((char*)p->data + 10, product, 16);
	p->data[28] = 0x08; // game

	apply_checksum(p);
}

void packet_audio_info(struct packet* p, size_t samplerate) {
	memset(p->header, 0, sizeof(p->header));
	memset(p->data, 0, sizeof(p->data));

	p->header[0] = 0x84;
	p->header[1] = 0x01;
	p->header[2] = 10;

	p->data[1] = 1 | (1 << 4);
	p->data[2] = 1 | (samplerate << 2);
	p->data[4] = 0;
	p->data[5] = (0 << 3) | (0 << 7);

	apply_checksum(p);
}

void packet_audio_clk_regen(struct packet* p, uint32_t n, uint32_t cts) {
	p->header[0] = 0x01;
	p->header[1] = 0x00;
	p->header[2] = 0x00;

	for(size_t k = 0; k < PACKET_BODY_LENGTH; k += PACKET_SUBLANE_LENGTH) {
		p->data[k + 0] = 0;
		p->data[k + 1] = (cts >> 16) & 0xFF;
		p->data[k + 2] = (cts >> 8) & 0xFF;
		p->data[k + 3] = cts & 0xFF;
		p->data[k + 4] = (n >> 16) & 0xFF;
		p->data[k + 5] = (n >> 8) & 0xFF;
		p->data[k + 6] = n & 0xFF;
	}
}