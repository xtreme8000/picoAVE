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

#include <assert.h>
#include <string.h>

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "utils.h"

#include "serializer.pio.h"
#include "tmds_serializer.h"

static uint pio_program_offset;

#define DUMMY_DATA_LENGTH 1024
static uint32_t dummy_data = 0xFF;

static void dma_tmds_configure(uint channel, uint other_channel, uint sm) {
	assert(channel != other_channel);

	dma_channel_config c = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
	channel_config_set_chain_to(&c, other_channel);
	channel_config_set_dreq(&c, pio_get_dreq(pio0, sm, true));
	channel_config_set_read_increment(&c, true);
	channel_config_set_write_increment(&c, false);
	dma_channel_configure(channel, &c, &pio0->txf[sm], &dummy_data,
						  DUMMY_DATA_LENGTH, false);
	dma_channel_set_irq0_enabled(channel, true);
}

struct tmds_data3* CORE0_CODE
tmds_serializer_transfer_callback(struct tmds_serializer* s) {
	for(size_t k = 0; k < 2; k++) {
		if(dma_channel_get_irq0_status(s->dma_channels[k])) {
			dma_channel_acknowledge_irq0(s->dma_channels[k]);

			struct tmds_data3* completed = s->dma_data[k];

			struct tmds_data data;
			if(likely(ffifo_pop(&s->queue_send, &data))) {
				dma_channel_config c
					= dma_get_channel_config(s->dma_channels[k]);
				channel_config_set_read_increment(&c, data.ptr != &dummy_data);
				dma_channel_set_config(s->dma_channels[k], &c, false);

				dma_channel_set_read_addr(s->dma_channels[k], data.ptr, false);
				dma_channel_set_trans_count(s->dma_channels[k], data.length,
											false);
				s->dma_data[k] = data.source;
			} else {
				panic_unsupported();
			}

			return completed;
		}
	}

	return NULL;
}

void tmds_serializer_init() {
	pio_program_offset = pio_add_program(pio0, &pio_serializer_program);
}

void tmds_serializer_create(struct tmds_serializer* s, uint gpio_base) {
	assert(s && gpio_base < 32 && (gpio_base % 2) == 0);

	s->pio_sm = pio_claim_unused_sm(pio0, true);
	pio_serializer_program_init(pio0, s->pio_sm, pio_program_offset, gpio_base);

	ffifo_init(&s->queue_send, TMDS_QUEUE_LENGTH);

	while(!ffifo_full(&s->queue_send)) {
		ffifo_push(&s->queue_send,
				   &(struct tmds_data) {
					   .ptr = &dummy_data,
					   .length = DUMMY_DATA_LENGTH,
					   .source = NULL,
				   });
	}

	for(size_t k = 0; k < 2; k++) {
		s->dma_channels[k] = dma_claim_unused_channel(true);
		s->dma_data[k] = NULL;
	}

	dma_tmds_configure(s->dma_channels[0], s->dma_channels[1], s->pio_sm);
	dma_tmds_configure(s->dma_channels[1], s->dma_channels[0], s->pio_sm);
	dma_channel_start(s->dma_channels[0]);
}

void tmds_serializer_wait_ready(struct tmds_serializer* s) {
	while(!pio_sm_is_tx_fifo_full(pio0, s->pio_sm))
		tight_loop_contents();
}