#include <assert.h>

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/platform.h"
#include "pico/stdlib.h"

#include "branch_hint.h"
#include "gpu_input.h"
#include "capture.pio.h"

FIFO_DEF(fifo_gpu, struct gpu_data*)

#define DUMMY_BUFFER_LENGTH 256
static uint32_t dummy_buffer[DUMMY_BUFFER_LENGTH];

struct gpu_input {
	size_t data_skipped;
	uint dma_channels[2];
	struct gpu_data* dma_data[2];
	struct fifo_gpu queue_unused;
	struct fifo_gpu queue_receive;
	uint pio_sm;
	uint pio_program_offset;
};

static struct gpu_input __scratch_x("dma1_data") gi;

static void __scratch_y("dma1") dma_isr1(void) {
	for(size_t k = 0; k < 2; k++) {
		if(dma_channel_get_irq1_status(gi.dma_channels[k])) {
			dma_channel_acknowledge_irq1(gi.dma_channels[k]);

			if(likely(gi.dma_data[k]))
				fifo_gpu_push(&gi.queue_receive, gi.dma_data + k);

			if(likely(fifo_gpu_pop(&gi.queue_unused, gi.dma_data + k))) {
				gi.dma_data[k]->data_skipped = gi.data_skipped;
				gi.data_skipped = 0;
				dma_channel_set_write_addr(gi.dma_channels[k],
										   gi.dma_data[k]->ptr, false);
				dma_channel_set_trans_count(gi.dma_channels[k],
											gi.dma_data[k]->length, false);
			} else {
				gi.dma_data[k] = NULL;
				gi.data_skipped += DUMMY_BUFFER_LENGTH;
				dma_channel_set_write_addr(gi.dma_channels[k], dummy_buffer,
										   false);
				dma_channel_set_trans_count(gi.dma_channels[k],
											DUMMY_BUFFER_LENGTH, false);
			}
		}
	}
}

static void dma_gpu_configure(uint channel, uint other_channel, uint sm,
							  struct gpu_data* data) {
	assert(channel != other_channel);

	dma_channel_config c = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
	channel_config_set_chain_to(&c, other_channel);
	channel_config_set_dreq(&c, pio_get_dreq(pio1, sm, false));
	channel_config_set_read_increment(&c, false);
	channel_config_set_write_increment(&c, true);
	dma_channel_configure(channel, &c, data->ptr, &pio1->rxf[sm], data->length,
						  false);
	dma_channel_set_irq1_enabled(channel, true);
}

void gpu_input_init(size_t capacity, size_t buffer_length, uint gpio_base) {
	assert(capacity > 2 && buffer_length > 0);

	gi.data_skipped = 0;

	fifo_gpu_init(&gi.queue_unused, capacity);
	fifo_gpu_init(&gi.queue_receive, capacity);

	while(!fifo_gpu_full(&gi.queue_unused)) {
		struct gpu_data* obj = malloc(sizeof(struct gpu_data));
		obj->length = buffer_length;
		obj->ptr = malloc(buffer_length * sizeof(uint32_t));
		fifo_gpu_push(&gi.queue_unused, &obj);
	}

	gi.pio_sm = pio_claim_unused_sm(pio1, true);
	gi.pio_program_offset = pio_add_program(pio1, &pio_capture_program);
	pio_capture_program_init(pio1, gi.pio_sm, gi.pio_program_offset, gpio_base);

	for(size_t k = 0; k < 2; k++) {
		gi.dma_channels[k] = dma_claim_unused_channel(true);
		fifo_gpu_pop(&gi.queue_unused, gi.dma_data + k);
	}

	dma_gpu_configure(gi.dma_channels[0], gi.dma_channels[1], gi.pio_sm,
					  gi.dma_data[0]);
	dma_gpu_configure(gi.dma_channels[1], gi.dma_channels[0], gi.pio_sm,
					  gi.dma_data[1]);

	irq_set_exclusive_handler(DMA_IRQ_1, dma_isr1);
	irq_set_priority(DMA_IRQ_1, 0);
	irq_set_enabled(DMA_IRQ_1, true);
}

void gpu_input_start() {
	pio_sm_set_enabled(pio1, gi.pio_sm, false);

	pio_sm_clear_fifos(pio1, gi.pio_sm);
	pio_sm_restart(pio1, gi.pio_sm);
	pio_sm_exec(pio1, gi.pio_sm, pio_encode_jmp(gi.pio_program_offset));

	dma_channel_start(gi.dma_channels[0]);
	pio_sm_set_enabled(pio1, gi.pio_sm, true);
}

struct gpu_data* gpu_input_receive() {
	struct gpu_data* d;
	fifo_gpu_pop_blocking(&gi.queue_receive, &d);
	return d;
}

void gpu_input_release(struct gpu_data* d) {
	fifo_gpu_push_blocking(&gi.queue_unused, &d);
}
