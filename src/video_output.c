#include "video_output.h"
#include "frame.h"
#include "packet_info_frame.h"
#include "pico/stdlib.h"
#include "tmds_clock.h"
#include "tmds_encode.h"
#include "tmds_serializer.h"
#include "utils.h"

FIFO_DEF(fifo_image, struct tmds_data3*)

struct video_output {
	struct tmds_clock channel_clock;
	struct tmds_serializer channels[TMDS_CHANNEL_COUNT];
	struct fifo_image pending_queue;
	struct fifo_image input_queue;
	queue_t* unused_queue;
	struct {
		bool active;
		int y;
	} state;
};

uint32_t tmds_vsync_pulse0[FRAME_WIDTH / 2];
uint32_t tmds_vsync_pulse1[FRAME_WIDTH / 2];
uint32_t tmds_vsync_pulse2[FRAME_WIDTH / 2];

uint32_t tmds_vsync_porch0[FRAME_WIDTH / 2];
uint32_t tmds_vsync_porch1[FRAME_WIDTH / 2];
uint32_t tmds_vsync_porch2[FRAME_WIDTH / 2];

uint32_t tmds_vsync_data0[FRAME_WIDTH / 2];
uint32_t tmds_vsync_data1[FRAME_WIDTH / 2];
uint32_t tmds_vsync_data2[FRAME_WIDTH / 2];

uint32_t tmds_image_porch0[FRAME_H_BLANK / 2];
uint32_t tmds_image_porch1[FRAME_H_BLANK / 2];
uint32_t tmds_image_porch2[FRAME_H_BLANK / 2];

static struct video_output CORE0_DATA vdo;
static struct tmds_data3 CORE0_DATA video_signal_parts[] = {
	{
		.allocated = false,
		.ptr = {tmds_vsync_porch0, tmds_vsync_porch1, tmds_vsync_porch2},
		.length = FRAME_WIDTH / 2,
	},
	{
		.allocated = false,
		.ptr = {tmds_vsync_pulse0, tmds_vsync_pulse1, tmds_vsync_pulse2},
		.length = FRAME_WIDTH / 2,
	},
	{
		.allocated = false,
		.ptr = {tmds_vsync_data0, tmds_vsync_data1, tmds_vsync_data2},
		.length = FRAME_WIDTH / 2,
	},
	{
		.allocated = false,
		.ptr = {tmds_image_porch0, tmds_image_porch1, tmds_image_porch2},
		.length = FRAME_H_BLANK / 2,
	},
};

static void build_sync_tables(void) {
	tmds_encode_sync(false, tmds_vsync_pulse0, tmds_vsync_pulse1,
					 tmds_vsync_pulse2);
	tmds_encode_sync(true, tmds_vsync_porch0, tmds_vsync_porch1,
					 tmds_vsync_porch2);
	tmds_encode_sync_video(tmds_image_porch0, tmds_image_porch1,
						   tmds_image_porch2);

	struct packet packets[2];
	packet_avi_info(packets + 0);
	packet_spd_info(packets + 1, "xtreme8k", "pico AVE");

	tmds_encode_sync(true, tmds_vsync_data0, tmds_vsync_data1,
					 tmds_vsync_data2);
	packets_encode(packets, 2, true, true, tmds_vsync_data0 + FRAME_H_BLANK,
				   tmds_vsync_data1 + FRAME_H_BLANK,
				   tmds_vsync_data2 + FRAME_H_BLANK);
}

static struct tmds_data3* CORE0_CODE build_video_signal(void) {
	struct tmds_data3* result = NULL;

	if(vdo.state.y < FRAME_V_PORCH_FRONT) {
		result = (vdo.state.y == 0) ? video_signal_parts + 2 :
									  video_signal_parts + 0;
		vdo.state.y++;
	} else if(vdo.state.y < FRAME_V_PORCH_FRONT + FRAME_V_SYNC) {
		result = video_signal_parts + 1;
		vdo.state.y++;
	} else if(vdo.state.y < FRAME_V_BLANK) {
		result = video_signal_parts + 0;
		vdo.state.y++;
	} else if(!vdo.state.active) {
		result = video_signal_parts + 3;
		vdo.state.active = true;
	} else if(fifo_image_pop(&vdo.input_queue, &result)) {
		if(!(vdo.state.y == FRAME_V_BLANK && !result->vsync)) {
			vdo.state.y++;
			vdo.state.active = false;
		}
	} else {
		// last resort, provide anything to keep serializers in sync
		gpio_set_mask(1 << PICO_DEFAULT_LED_PIN);
		result = video_signal_parts + 0;
		vdo.state.y++;
	}

	if(vdo.state.y >= FRAME_HEIGHT)
		vdo.state.y = 0;

	return result;
}

static void CORE0_CODE refill_channels(void) {
	while(1) {
		for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++) {
			if(ffifo_full(&vdo.channels[k].queue_send))
				return;
		}

		struct tmds_data3* data = build_video_signal();

		if(data) {
			fifo_image_push(&vdo.pending_queue, &data);

			for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++) {
				ffifo_push(&vdo.channels[k].queue_send,
						   &(struct tmds_data) {.ptr = data->ptr[k],
												.length = data->length});
				vdo.channels[k].pending_transfers++;
			}
		}
	}
}

static void CORE0_CODE release_sent_entries(void) {
	while(1) {
		bool transfer_complete = true;

		for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++) {
			if(vdo.channels[k].pending_transfers < 3) {
				transfer_complete = false;
				break;
			}
		}

		if(!transfer_complete)
			break;

		struct tmds_data3* data;
		if(fifo_image_pop(&vdo.pending_queue, &data)) {
			for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++)
				vdo.channels[k].pending_transfers--;

			if(data->allocated)
				queue_add_blocking(vdo.unused_queue, &data);
		}
	}
}

static void CORE0_CODE dma_isr0(void) {
	for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++)
		tmds_serializer_transfer_callback(vdo.channels + k);

	release_sent_entries();
	refill_channels();
}

void video_output_init(uint gpio_channels[3], uint gpio_clk, size_t capacity,
					   queue_t* unused_queue) {
	assert(capacity > 0);

	build_sync_tables();

	fifo_image_init(&vdo.input_queue, capacity);
	fifo_image_init(&vdo.pending_queue, TMDS_QUEUE_LENGTH * 2);
	vdo.unused_queue = unused_queue;
	vdo.state.y = 0;
	vdo.state.active = false;

	tmds_serializer_init();
	tmds_clock_init();

	for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++) {
		tmds_serializer_create(vdo.channels + k, gpio_channels[k]);
		tmds_serializer_wait_ready(vdo.channels + k);
	}

	tmds_clock_create(&vdo.channel_clock, gpio_clk);

	irq_set_exclusive_handler(DMA_IRQ_0, dma_isr0);
	irq_set_priority(DMA_IRQ_0, 0);
	irq_set_enabled(DMA_IRQ_0, true);
}

void video_output_start() {
	tmds_start_with_clock(&vdo.channel_clock, vdo.channels, TMDS_CHANNEL_COUNT);
}

void video_output_submit(struct tmds_data3* data) {
	assert(data);
	fifo_image_push_blocking(&vdo.input_queue, &data);
}