#include <limits.h>

#include "frame.h"
#include "packet_info_frame.h"
#include "pico/stdlib.h"
#include "tmds_clock.h"
#include "tmds_encode.h"
#include "tmds_serializer.h"
#include "utils.h"
#include "video_output.h"

FIFO_DEF(fifo_image, struct tmds_data3*)

#define RATE_THRESHOLD (FRAME_HEIGHT * 60 * 4 * 4096)
#define RATE_INCREASE(clk) ((clk) * 4096)

struct video_output {
	struct tmds_clock channel_clock;
	struct tmds_serializer channels[TMDS_CHANNEL_COUNT];
	struct fifo_image input_queue_video;
	struct fifo_image input_queue_packets;
	struct mem_pool* pool_video;
	struct mem_pool* pool_packets;
	struct {
		uint32_t rate_increase;
		uint32_t rate_current;
		bool active;
		size_t y;
	} state;
	struct tmds_data3 audio_info_packets[2];
	size_t audio_info_idx;
};

static uint32_t tmds_vsync_pulse0[FRAME_WIDTH / 2];
static uint32_t tmds_vsync_pulse1[FRAME_WIDTH / 2];
static uint32_t tmds_vsync_pulse2[FRAME_WIDTH / 2];

static uint32_t tmds_vsync_porch0[FRAME_WIDTH / 2];
static uint32_t tmds_vsync_porch1[FRAME_WIDTH / 2];
static uint32_t tmds_vsync_porch2[FRAME_WIDTH / 2];

static struct video_output CORE0_DATA vdo;
static struct tmds_data3 CORE0_DATA video_signal_parts[] = {
	{
		.type = TYPE_CONST,
		.ptr = {tmds_vsync_porch0 + (FRAME_WIDTH - FRAME_BUFFER_WIDTH) / 2,
				tmds_vsync_porch1 + (FRAME_WIDTH - FRAME_BUFFER_WIDTH) / 2,
				tmds_vsync_porch2 + (FRAME_WIDTH - FRAME_BUFFER_WIDTH) / 2},
		.length = FRAME_BUFFER_WIDTH / 2,
	},
	{
		.type = TYPE_CONST,
		.ptr = {tmds_vsync_pulse0, tmds_vsync_pulse1, tmds_vsync_pulse2},
		.length = FRAME_WIDTH / 2,
	},
	{
		.type = TYPE_CONST,
		.ptr = {tmds_vsync_porch0, tmds_vsync_porch1, tmds_vsync_porch2},
		.length = (FRAME_WIDTH - FRAME_BUFFER_WIDTH) / 2,
	},
	{
		.type = TYPE_CONST,
		.ptr = {tmds_vsync_porch0, tmds_vsync_porch1, tmds_vsync_porch2},
		.length = FRAME_WIDTH / 2,
	},
};

static void build_sync_tables(void) {
	tmds_encode_sync(FRAME_WIDTH, false, tmds_vsync_pulse0, tmds_vsync_pulse1,
					 tmds_vsync_pulse2);
	tmds_encode_sync(FRAME_WIDTH, true, tmds_vsync_porch0, tmds_vsync_porch1,
					 tmds_vsync_porch2);

	struct packet packets[2];
	packet_avi_info(packets + 0);
	packet_spd_info(packets + 1, "picoAVE", "picoAVE");

	packets_encode(packets, sizeof(packets) / sizeof(*packets), true, false,
				   tmds_vsync_pulse0 + FRAME_H_BLANK / 2,
				   tmds_vsync_pulse1 + FRAME_H_BLANK / 2,
				   tmds_vsync_pulse2 + FRAME_H_BLANK / 2);
}

static struct tmds_data3* CORE0_CODE build_video_signal(void) {
	struct tmds_data3* result = NULL;

	if(!vdo.state.active)
		vdo.state.rate_current += vdo.state.rate_increase;

	if(vdo.state.y >= FRAME_V_PORCH_FRONT
	   && vdo.state.y < FRAME_V_PORCH_FRONT + FRAME_V_SYNC) {
		result = video_signal_parts + 1;
		vdo.state.y++;
	} else if(vdo.state.y < FRAME_V_BLANK) {
		if(vdo.state.active) {
			result = video_signal_parts + 0;
			vdo.state.active = false;
			vdo.state.y++;
		} else {
			if(vdo.state.y == 0) {
				result = vdo.audio_info_packets + vdo.audio_info_idx;
				vdo.state.active = true;
			} else if(vdo.state.rate_current >= RATE_THRESHOLD
					  && fifo_image_pop(&vdo.input_queue_packets, &result)) {
				vdo.state.rate_current -= RATE_THRESHOLD;
				vdo.state.active = true;
			} else {
				result = video_signal_parts + 3;
				vdo.state.y++;
			}
		}
	} else if(!vdo.state.active) {
		if(vdo.state.rate_current >= RATE_THRESHOLD
		   && fifo_image_pop(&vdo.input_queue_packets, &result)) {
			vdo.state.rate_current -= RATE_THRESHOLD;
		} else {
			result = video_signal_parts + 2;
		}
		vdo.state.active = true;
	} else if(fifo_image_pop(&vdo.input_queue_video, &result)) {
		if(!(vdo.state.y == FRAME_V_BLANK && !result->vsync)) {
			vdo.state.y++;
			vdo.state.active = false;
		}
	}

	if(!result) {
		// last resort, provide anything to keep serializers in sync
		result = video_signal_parts + 0;
		vdo.state.y++;
		vdo.state.active = false;
		vdo.state.rate_current = 0;
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
			for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++)
				ffifo_push(
					&vdo.channels[k].queue_send,
					&(struct tmds_data) {
						.ptr = data->ptr[k],
						.length = data->length,
						.source = (data->type != TYPE_CONST) ? data : NULL,
					});
		}
	}
}

static void CORE0_CODE dma_isr0(void) {
	struct tmds_data3* completed[TMDS_CHANNEL_COUNT];

	// service serializers first, then release data later
	for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++)
		completed[k] = tmds_serializer_transfer_callback(vdo.channels + k);

	for(size_t k = 0; k < TMDS_CHANNEL_COUNT; k++) {
		if(completed[k] && (++completed[k]->transfers) >= TMDS_CHANNEL_COUNT) {
			switch(completed[k]->type) {
				case TYPE_VIDEO:
					mem_pool_free(vdo.pool_video, completed[k]);
					break;
				case TYPE_PACKET:
					mem_pool_free(vdo.pool_packets, completed[k]);
					break;
				default:
			}
		}
	}

	refill_channels();
}

void video_output_init(uint gpio_channels[TMDS_CHANNEL_COUNT], uint gpio_clk,
					   struct mem_pool* pool_video,
					   struct mem_pool* pool_packets) {
	assert(pool_video && pool_packets);

	build_sync_tables();

	fifo_image_init(&vdo.input_queue_video, mem_pool_capacity(pool_video));
	fifo_image_init(&vdo.input_queue_packets, mem_pool_capacity(pool_packets));
	vdo.pool_video = pool_video;
	vdo.pool_packets = pool_packets;

	vdo.state.rate_current = 0;
	vdo.state.y = 0;
	vdo.state.active = false;
	vdo.audio_info_idx = 0;

	for(size_t k = 0; k < 2; k++) {
		size_t len = (FRAME_WIDTH - FRAME_BUFFER_WIDTH) / 2;
		vdo.audio_info_packets[k] = (struct tmds_data3) {
			.type = TYPE_CONST,
			.length = len,
			.ptr
			= {malloc(len * sizeof(uint32_t)), malloc(len * sizeof(uint32_t)),
			   malloc(len * sizeof(uint32_t))},
		};

		tmds_encode_sync(len * 2, true, vdo.audio_info_packets[k].ptr[0],
						 vdo.audio_info_packets[k].ptr[1],
						 vdo.audio_info_packets[k].ptr[2]);
	}

	video_output_set_audio_info(48000);

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

	if(data->type != TYPE_CONST)
		data->transfers = 0;

	switch(data->type) {
		case TYPE_CONST:
		case TYPE_VIDEO:
			fifo_image_push_blocking(&vdo.input_queue_video, &data);
			break;
		case TYPE_PACKET:
			fifo_image_push_blocking(&vdo.input_queue_packets, &data);
			break;
	}
}

// must be in this order to match audio info frame
static int common_samplerates[][2] = {
	{32000, 4096},	{44100, 6272},	 {48000, 6144},	  {88200, 12544},
	{96000, 12288}, {176400, 25088}, {192000, 24576},
};

static uint32_t audio_cts(uint32_t tmds_clk, uint32_t n, uint32_t samplerate) {
	return ((uint64_t)tmds_clk * 100 * (uint64_t)n / 128)
		/ (uint64_t)samplerate;
}

void video_output_set_audio_info(uint32_t samplerate) {
	// limit samplerate between -5% of 32kHz and +5% of 192kHz
	uint32_t sr_clamped
		= clamp_n(samplerate, 32000 * 95 / 100, 192000 * 105 / 100);

	size_t best_match = 0;
	int best_dist = INT_MAX;

	for(size_t k = 0;
		k < sizeof(common_samplerates) / sizeof(*common_samplerates); k++) {
		int dist = abs(common_samplerates[k][0] - (int)sr_clamped);

		if(dist < best_dist) {
			best_match = k;
			best_dist = dist;
		}
	}

	size_t other_packet = 1 - vdo.audio_info_idx;

	struct packet p;
	packet_audio_info(&p, best_match + 1);

	packets_encode(
		&p, 1, false, true,
		vdo.audio_info_packets[other_packet].ptr[0] + FRAME_H_PORCH_FRONT / 2,
		vdo.audio_info_packets[other_packet].ptr[1] + FRAME_H_PORCH_FRONT / 2,
		vdo.audio_info_packets[other_packet].ptr[2] + FRAME_H_PORCH_FRONT / 2);

	uint32_t audio_n = common_samplerates[best_match][1];
	packet_audio_clk_regen(&p, audio_n,
						   audio_cts(FRAME_CLOCK, audio_n, sr_clamped));

	packets_encode(&p, 1, true, true,
				   vdo.audio_info_packets[other_packet].ptr[0]
					   + (FRAME_H_PORCH_FRONT + FRAME_H_SYNC) / 2,
				   vdo.audio_info_packets[other_packet].ptr[1]
					   + (FRAME_H_PORCH_FRONT + FRAME_H_SYNC) / 2,
				   vdo.audio_info_packets[other_packet].ptr[2]
					   + (FRAME_H_PORCH_FRONT + FRAME_H_SYNC) / 2);

	vdo.audio_info_idx = other_packet;
	vdo.state.rate_increase = RATE_INCREASE(sr_clamped);
}