#include <assert.h>
#include <string.h>

#include "branch_hint.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/platform.h"
#include "pico/stdlib.h"

#include "serializer.pio.h"
#include "tmds_serializer.h"

static uint pio_program_offset;
static uint32_t dummy_data[32];

static void dma_tmds_configure(uint channel, uint other_channel, uint sm) {
	assert(channel != other_channel);

	dma_channel_config c = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
	channel_config_set_chain_to(&c, other_channel);
	channel_config_set_dreq(&c, pio_get_dreq(pio0, sm, true));
	channel_config_set_read_increment(&c, true);
	channel_config_set_write_increment(&c, false);
	dma_channel_configure(channel, &c, &pio0->txf[sm], dummy_data,
						  sizeof(dummy_data) / sizeof(*dummy_data), false);
	dma_channel_set_irq0_enabled(channel, true);
}

bool __scratch_x("dma0")
	tmds_serializer_transfer_callback(struct tmds_serializer* s) {
	for(size_t k = 0; k < 2; k++) {
		if(dma_channel_get_irq0_status(s->dma_channels[k])) {
			dma_channel_acknowledge_irq0(s->dma_channels[k]);

			// TODO: needs LTO enabled, otherwise misaligns everything because
			// of initial i-cache miss on (queue-)function calls
			struct tmds_data data;
			if(likely(ffifo_pop(&s->queue_send, &data))) {
				dma_channel_set_read_addr(s->dma_channels[k], data.ptr, false);
				dma_channel_set_trans_count(s->dma_channels[k], data.length,
											false);
			} else {
				gpio_set_mask(1 << PICO_DEFAULT_LED_PIN);
				panic("must not be reached\n");
			}

			return true;
		}
	}

	return false;
}

void tmds_serializer_init() {
	memset(dummy_data, 0xFF, sizeof(dummy_data));
	pio_program_offset = pio_add_program(pio0, &pio_serializer_program);
}

void tmds_serializer_create(struct tmds_serializer* s, uint gpio_base) {
	assert(s && gpio_base < 32 && (gpio_base % 2) == 0);

	s->pio_sm = pio_claim_unused_sm(pio0, true);
	pio_serializer_program_init(pio0, s->pio_sm, pio_program_offset, gpio_base);

	ffifo_init(&s->queue_send, TMDS_QUEUE_LENGTH);

	s->pending_transfers = 0;

	while(!ffifo_full(&s->queue_send)) {
		ffifo_push(&s->queue_send,
				   &(struct tmds_data) {.ptr = dummy_data,
										.length = sizeof(dummy_data)
											/ sizeof(*dummy_data)});
		s->pending_transfers--;
	}

	for(size_t k = 0; k < 2; k++)
		s->dma_channels[k] = dma_claim_unused_channel(true);

	dma_tmds_configure(s->dma_channels[0], s->dma_channels[1], s->pio_sm);
	dma_tmds_configure(s->dma_channels[1], s->dma_channels[0], s->pio_sm);
	dma_channel_start(s->dma_channels[0]);
}

void tmds_serializer_wait_ready(struct tmds_serializer* s) {
	while(!pio_sm_is_tx_fifo_full(pio0, s->pio_sm))
		tight_loop_contents();
}