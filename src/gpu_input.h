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

#ifndef GPU_INPUT_H
#define GPU_INPUT_H

#include "mem_pool.h"
#include "pico/platform.h"

struct gpu_data {
	bool data_skipped;
	size_t length;
	uint32_t* ptr;
};

#define AUDIO_FRAME_LENGTH 192

struct audio_data {
	uint32_t samplerate;
	uint32_t audio_data[AUDIO_FRAME_LENGTH];
};

void gpu_input_init(size_t capacity, size_t buffer_length, uint video_base,
					struct mem_pool* pool_audio, queue_t* receive_queue_audio,
					uint audio_base, uint audio_ws);
void gpu_input_start(void);
struct gpu_data* gpu_input_receive(void);
void gpu_input_release(struct gpu_data* d);
void gpu_input_drain(bool shifted);

#endif