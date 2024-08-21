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

#ifndef TMDS_SERIALIZER_H
#define TMDS_SERIALIZER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ffifo.h"
#include "pico/platform.h"

#define TMDS_QUEUE_LENGTH 8

struct tmds_data3;

struct tmds_data {
	uint32_t* ptr;
	size_t length;
	struct tmds_data3* source;
};

FIFO_DEF(ffifo, struct tmds_data)

struct tmds_serializer {
	uint dma_channels[2];
	struct tmds_data3* dma_data[2];
	struct ffifo queue_send;
	uint pio_sm;
};

void tmds_serializer_init(void);
void tmds_serializer_create(struct tmds_serializer* s, uint gpio_base);
void tmds_serializer_wait_ready(struct tmds_serializer* s);
struct tmds_data3* tmds_serializer_transfer_callback(struct tmds_serializer* s);

#endif