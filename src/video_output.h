#ifndef VIDEO_OUTPUT_H
#define VIDEO_OUTPUT_H

#include "pico/util/queue.h"
#include "tmds_serializer.h"

#define TMDS_CHANNEL_COUNT 3

struct tmds_data3 {
	bool vsync;
	bool allocated;
	size_t vis_length;
	size_t length;
	uint32_t* ptr[TMDS_CHANNEL_COUNT];
};

void video_output_init(uint gpio_channels[3], uint gpio_clk, size_t capacity,
					   queue_t* unused_queue);
void video_output_start(void);
void video_output_submit(struct tmds_data3* data);

#endif