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

#ifndef VIDEO_OUTPUT_H
#define VIDEO_OUTPUT_H

#include "mem_pool.h"
#include "pico/util/queue.h"
#include "tmds_serializer.h"

#define TMDS_CHANNEL_COUNT 3

struct tmds_data3 {
	enum tmds_data_type {
		TYPE_CONST,
		TYPE_VIDEO,
		TYPE_PACKET,
	} type;
	bool vsync;
	size_t encode_offset;
	size_t encode_length;
	size_t length;
	uint32_t* ptr[TMDS_CHANNEL_COUNT];
	size_t transfers;
};

void video_output_init(uint gpio_channels[TMDS_CHANNEL_COUNT], uint gpio_clk,
					   struct mem_pool* pool_video,
					   struct mem_pool* pool_packets);
void video_output_start(void);
void video_output_submit(struct tmds_data3* data);
void video_output_set_audio_info(uint32_t samplerate);

#endif