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
	bool last_line;
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