#include <assert.h>

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "capture.pio.h"
#include "frame.h"
#include "gpu_input.h"
#include "i2s.pio.h"
#include "utils.h"
#include "video_output.h"

FIFO_DEF(fifo_gpu, struct gpu_data*)

#define DUMMY_BUFFER_LENGTH 256
static uint32_t dummy_buffer[DUMMY_BUFFER_LENGTH];

struct gpu_input {
	struct {
		size_t data_skipped;
		uint dma_channels[2];
		struct gpu_data* dma_data[2];
		struct fifo_gpu queue_unused;
		struct fifo_gpu queue_receive;
		uint pio_sm;
		uint pio_program_offset;
	} video;
	struct {
		uint dma_channels[2];
		struct tmds_data3* dma_data[2];
		struct mem_pool* pool_audio;
		queue_t* queue_receive;
		uint pio_sm;
		uint pio_program_offset;
	} audio;
};

static struct gpu_input CORE1_DATA gi;

static void CORE1_CODE dma_isr1(void) {
	// video

	for(size_t k = 0; k < 2; k++) {
		if(dma_channel_get_irq1_status(gi.video.dma_channels[k])) {
			dma_channel_acknowledge_irq1(gi.video.dma_channels[k]);

			if(likely(gi.video.dma_data[k]))
				fifo_gpu_push(&gi.video.queue_receive, gi.video.dma_data + k);

			if(likely(fifo_gpu_pop(&gi.video.queue_unused,
								   gi.video.dma_data + k))) {
				gi.video.dma_data[k]->data_skipped = gi.video.data_skipped;
				gi.video.data_skipped = 0;
				dma_channel_set_write_addr(gi.video.dma_channels[k],
										   gi.video.dma_data[k]->ptr, false);
				dma_channel_set_trans_count(gi.video.dma_channels[k],
											gi.video.dma_data[k]->length,
											false);
			} else {
				gi.video.dma_data[k] = NULL;
				gi.video.data_skipped += DUMMY_BUFFER_LENGTH;
				dma_channel_set_write_addr(gi.video.dma_channels[k],
										   dummy_buffer, false);
				dma_channel_set_trans_count(gi.video.dma_channels[k],
											DUMMY_BUFFER_LENGTH, false);
			}
		}
	}

	// audio

	for(size_t k = 0; k < 2; k++) {
		if(dma_channel_get_irq1_status(gi.audio.dma_channels[k])) {
			dma_channel_acknowledge_irq1(gi.audio.dma_channels[k]);

			if(likely(gi.audio.dma_data[k]))
				queue_add_blocking(gi.audio.queue_receive,
								   gi.audio.dma_data + k);

			gi.audio.dma_data[k] = mem_pool_try_alloc(gi.audio.pool_audio);

			if(likely(gi.audio.dma_data[k])) {
				dma_channel_set_write_addr(gi.audio.dma_channels[k],
										   gi.audio.dma_data[k]->audio_data,
										   false);
				dma_channel_set_trans_count(gi.audio.dma_channels[k],
											gi.audio.dma_data[k]->audio_length,
											false);
			} else {
				dma_channel_set_write_addr(gi.audio.dma_channels[k],
										   dummy_buffer, false);
				dma_channel_set_trans_count(gi.audio.dma_channels[k],
											DUMMY_BUFFER_LENGTH, false);
			}
		}
	}
}

static void dma_gpu_configure(uint channel, uint other_channel, uint sm,
							  void* ptr, size_t length) {
	assert(channel != other_channel);

	dma_channel_config c = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
	channel_config_set_chain_to(&c, other_channel);
	channel_config_set_dreq(&c, pio_get_dreq(pio1, sm, false));
	channel_config_set_read_increment(&c, false);
	channel_config_set_write_increment(&c, true);
	dma_channel_configure(channel, &c, ptr, &pio1->rxf[sm], length, false);
	dma_channel_set_irq1_enabled(channel, true);
}

void gpu_input_init(size_t capacity, size_t buffer_length, uint video_base,
					struct mem_pool* pool_audio, queue_t* receive_queue_audio,
					uint audio_base, uint audio_ws) {
	assert(capacity > 2 && buffer_length > 0);

	// video

	gi.video.data_skipped = 0;

	fifo_gpu_init(&gi.video.queue_unused, capacity);
	fifo_gpu_init(&gi.video.queue_receive, capacity);

	while(!fifo_gpu_full(&gi.video.queue_unused)) {
		struct gpu_data* obj = malloc(sizeof(struct gpu_data));
		obj->length = buffer_length;
		obj->ptr = malloc(buffer_length * sizeof(uint32_t));
		fifo_gpu_push(&gi.video.queue_unused, &obj);
	}

	gi.video.pio_sm = pio_claim_unused_sm(pio1, true);
	gi.video.pio_program_offset = pio_add_program(pio1, &pio_capture_program);
	pio_capture_program_init(pio1, gi.video.pio_sm, gi.video.pio_program_offset,
							 video_base);

	for(size_t k = 0; k < 2; k++) {
		gi.video.dma_channels[k] = dma_claim_unused_channel(true);
		fifo_gpu_pop(&gi.video.queue_unused, gi.video.dma_data + k);
	}

	dma_gpu_configure(gi.video.dma_channels[0], gi.video.dma_channels[1],
					  gi.video.pio_sm, gi.video.dma_data[0]->ptr,
					  gi.video.dma_data[0]->length);
	dma_gpu_configure(gi.video.dma_channels[1], gi.video.dma_channels[0],
					  gi.video.pio_sm, gi.video.dma_data[1]->ptr,
					  gi.video.dma_data[1]->length);

	// audio

	gi.audio.pool_audio = pool_audio;
	gi.audio.queue_receive = receive_queue_audio;

	gi.audio.pio_sm = pio_claim_unused_sm(pio1, true);
	gi.audio.pio_program_offset = pio_add_program(pio1, &pio_i2s_program);
	pio_i2s_program_init(pio1, gi.audio.pio_sm, gi.audio.pio_program_offset,
						 audio_base, audio_ws);

	for(size_t k = 0; k < 2; k++) {
		gi.audio.dma_channels[k] = dma_claim_unused_channel(true);
		gi.audio.dma_data[k] = mem_pool_alloc(gi.audio.pool_audio);
	}

	dma_gpu_configure(gi.audio.dma_channels[0], gi.audio.dma_channels[1],
					  gi.audio.pio_sm, gi.audio.dma_data[0]->audio_data,
					  gi.audio.dma_data[0]->audio_length);
	dma_gpu_configure(gi.audio.dma_channels[1], gi.audio.dma_channels[0],
					  gi.audio.pio_sm, gi.audio.dma_data[1]->audio_data,
					  gi.audio.dma_data[1]->audio_length);

	irq_set_exclusive_handler(DMA_IRQ_1, dma_isr1);
	irq_set_priority(DMA_IRQ_1, 0);
	irq_set_enabled(DMA_IRQ_1, true);
}

void gpu_input_start() {
	// video

	pio_sm_set_enabled(pio1, gi.video.pio_sm, false);

	pio_sm_clear_fifos(pio1, gi.video.pio_sm);
	pio_sm_restart(pio1, gi.video.pio_sm);
	pio_sm_exec(pio1, gi.video.pio_sm,
				pio_encode_jmp(gi.video.pio_program_offset));

	dma_channel_start(gi.video.dma_channels[0]);
	pio_sm_set_enabled(pio1, gi.video.pio_sm, true);

	// audio

	pio_sm_set_enabled(pio1, gi.audio.pio_sm, false);

	pio_sm_clear_fifos(pio1, gi.audio.pio_sm);
	pio_sm_restart(pio1, gi.audio.pio_sm);
	pio_sm_exec(pio1, gi.audio.pio_sm,
				pio_encode_jmp(gi.audio.pio_program_offset));

	dma_channel_start(gi.audio.dma_channels[0]);
	pio_sm_set_enabled(pio1, gi.audio.pio_sm, true);
}

struct gpu_data* gpu_input_receive() {
	struct gpu_data* d;
	fifo_gpu_pop_blocking(&gi.video.queue_receive, &d);
	return d;
}

void gpu_input_release(struct gpu_data* d) {
	fifo_gpu_push_blocking(&gi.video.queue_unused, &d);
}
