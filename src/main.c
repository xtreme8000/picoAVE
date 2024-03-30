#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/structs/bus_ctrl.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "frame.h"
#include "gpu_input.h"
#include "packets.h"
#include "tmds_encode.h"
#include "utils.h"
#include "video_output.h"

uint32_t tmds_image_00h[FRAME_VIS_WIDTH / 2];
uint32_t tmds_image_10h[FRAME_VIS_WIDTH / 2];
uint32_t tmds_image_80h[FRAME_VIS_WIDTH / 2];

queue_t queue_unused;
queue_t queue_test;

void thread2(void);

int main() {
	// set_sys_clock_pll(336 * 1000 * 1000, 7, 6); // 8 MHz
	vreg_set_voltage(VREG_VOLTAGE_1_20);
	sleep_ms(10);
	set_sys_clock_khz(FRAME_CLOCK, true);
	sleep_ms(10);

	// Looks like main memory has high congestion in hotspots, this gives
	// DMA accesses the highest priority to keep all TMDS data channels in sync.
	// Additonally most interrupt code + data has been moved to scratch memory.
	bus_ctrl_hw->priority
		= BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

	// stdio_init_all();
	// sleep_ms(5000);

	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

	for(size_t k = 0; k < FRAME_VIS_WIDTH / 2; k++) {
		tmds_image_00h[k] = 0xffd00;
		tmds_image_10h[k] = tmds_symbols_10h[4];
		tmds_image_80h[k] = tmds_symbols_80h[4];
	}

	queue_init(&queue_unused, sizeof(struct tmds_data3*), 16);
	queue_init(&queue_test, sizeof(struct tmds_data3*), 16);

	for(int k = 0; k < queue_unused.element_count; k++) {
		struct tmds_data3* obj = malloc(sizeof(struct tmds_data3));
		obj->allocated = true;
		obj->encode_offset = 0;
		obj->encode_length = 0;
		obj->length = FRAME_VIS_WIDTH / 2;
		obj->ptr[0] = tmds_image_00h;
		obj->ptr[1] = malloc(obj->length * sizeof(uint32_t));
		obj->ptr[2] = malloc(obj->length * sizeof(uint32_t));

		queue_add_blocking(&queue_unused, &obj);
	}

	tmds_encode_init();
	tmds_encode_setup();
	packets_init();
	video_output_init((uint[]) {18, 20, 26}, 16, 8, &queue_unused);

	multicore_launch_core1(thread2);
	video_output_start();

	while(1) {
		struct tmds_data3* obj;
		queue_remove_blocking(&queue_test, &obj);

		if(obj->encode_length > 0) {
			size_t bias = tmds_encode_video(obj->ptr[2] + obj->encode_offset,
											true, obj->encode_length,
											obj->ptr[2] + obj->encode_offset);

			if(obj->encode_offset + obj->encode_length < obj->length)
				obj->ptr[2][obj->encode_offset + obj->encode_length]
					= tmds_symbols_80h[bias];
		}

		video_output_submit(obj);
	}

	return 0;
}

size_t min_n(size_t a, size_t b) {
	return a < b ? a : b;
}

void advance_input(size_t* idx, struct gpu_data** data, size_t amount) {
	*idx += amount;

	while(*idx >= (*data)->length) {
		*idx -= (*data)->length;
		gpu_input_release(*data);
		*data = gpu_input_receive();
	}
}

bool find_image_start(struct gpu_data** data, size_t* start, size_t* width) {
	struct gpu_data* current_data = gpu_input_receive();

	bool prev_blanking = false;
	size_t video_start, video_end;
	size_t k = 0;

	while(1) {
		bool blanking = (current_data->ptr[k] & 0xFF00FF00) == 0;

		if(unlikely(prev_blanking && !blanking)) {
			video_start = k;
			break;
		}

		prev_blanking = blanking;

		if(unlikely((++k) >= current_data->length))
			return false;
	}

	while(1) {
		bool blanking = (current_data->ptr[k] & 0xFF00FF00) == 0;

		if(unlikely(blanking)) {
			video_end = k;
			break;
		}

		if(unlikely((++k) >= current_data->length))
			return false;
	}

	*start = video_start;
	*width = min_n(video_end - video_start, FRAME_VIS_WIDTH / 2);
	*data = current_data;
	return true;
}

struct tmds_data3 empty_line;
struct tmds_data3 empty_line_sync;

void thread2() {
	empty_line = empty_line_sync = (struct tmds_data3) {
		.allocated = false,
		.encode_length = 0,
		.length = FRAME_VIS_WIDTH / 2,
		.ptr = {tmds_image_00h, tmds_image_10h, tmds_image_80h},
	};

	empty_line.vsync = false;
	empty_line_sync.vsync = true;

	tmds_encode_setup();

	// TODO: only 32 because of an overflow during following hsync scan
	gpu_input_init(32, FRAME_WIDTH / 2 * 2, 2);
	gpu_input_start();

	while(!queue_is_full(&queue_unused))
		tight_loop_contents();

	for(int k = 0; k < queue_unused.element_count; k++) {
		struct tmds_data3* obj;
		queue_remove_blocking(&queue_unused, &obj);

		memcpy(obj->ptr[1], tmds_image_10h, sizeof(tmds_image_10h));
		memcpy(obj->ptr[2], tmds_image_80h, sizeof(tmds_image_80h));

		queue_add_blocking(&queue_unused, &obj);
	}

	for(int k = 0; k < FRAME_HEIGHT / 3 * 60; k++) {
		struct gpu_data* in = gpu_input_receive();
		gpu_input_release(in);
	}

	size_t current_idx, video_width;
	struct gpu_data* current_data;
	find_image_start(&current_data, &current_idx, &video_width);

	static_assert(FRAME_VIS_WIDTH / 2 % 3 == 0);
	size_t video_width_padded = (video_width + 2) / 3 * 3;
	size_t video_xstart = (FRAME_VIS_WIDTH / 2 - video_width) / 2;

	if(video_xstart + video_width_padded > FRAME_VIS_WIDTH / 2)
		video_xstart = 0;

	bool prev_blank = false;

	while(1) {
		bool blanking = (current_data->ptr[current_idx] & 0xFF00FF00) == 0;

		if(prev_blank && !blanking)
			break;

		prev_blank = blanking;
		advance_input(&current_idx, &current_data, FRAME_WIDTH / 2);
	}

	size_t video_height = 0;

	while(1) {
		bool blanking = (current_data->ptr[current_idx] & 0xFF00FF00) == 0;

		if(blanking)
			break;

		advance_input(&current_idx, &current_data, FRAME_WIDTH / 2);
		video_height++;
	}

	advance_input(&current_idx, &current_data,
				  (FRAME_HEIGHT - video_height) * FRAME_WIDTH / 2);

	video_height = min_n(video_height, FRAME_VIS_HEIGHT);
	size_t video_vstart = (FRAME_VIS_HEIGHT - video_height) / 2;
	size_t video_vend = video_height + video_vstart;

	while(1) {
		for(size_t k = video_vstart; k < video_vend; k++) {
			struct tmds_data3* obj;
			queue_remove_blocking(&queue_unused, &obj);
			obj->vsync = k == 0;
			obj->encode_offset = video_xstart;
			obj->encode_length = video_width_padded;

			size_t line_idx = 0;

			while(line_idx < video_width) {
				size_t can_take = min_n(video_width - line_idx,
										current_data->length - current_idx);
				memcpy(obj->ptr[2] + video_xstart + line_idx,
					   current_data->ptr + current_idx,
					   can_take * sizeof(uint32_t));

				line_idx += can_take;
				advance_input(&current_idx, &current_data, can_take);
			}

			while(line_idx < video_width_padded)
				obj->ptr[2][video_xstart + line_idx++] = 0x10801080;

			size_t bias = tmds_encode_video(obj->ptr[2] + video_xstart, false,
											video_width_padded,
											obj->ptr[1] + video_xstart);

			if(video_xstart + video_width_padded < obj->length)
				obj->ptr[1][video_xstart + video_width_padded]
					= tmds_symbols_10h[bias];

			queue_add_blocking(&queue_test, &obj);
			advance_input(&current_idx, &current_data,
						  FRAME_WIDTH / 2 - video_width);
		}

		for(size_t k = video_vend; k < FRAME_VIS_HEIGHT; k++) {
			struct tmds_data3* obj = (k == 0) ? &empty_line_sync : &empty_line;
			queue_add_blocking(&queue_test, &obj);
			advance_input(&current_idx, &current_data, FRAME_WIDTH / 2);
		}

		advance_input(&current_idx, &current_data,
					  FRAME_V_BLANK * FRAME_WIDTH / 2);

		for(size_t k = 0; k < video_vstart; k++) {
			struct tmds_data3* obj = (k == 0) ? &empty_line_sync : &empty_line;
			queue_add_blocking(&queue_test, &obj);
			advance_input(&current_idx, &current_data, FRAME_WIDTH / 2);
		}
	}
}