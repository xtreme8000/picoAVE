#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/structs/bus_ctrl.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "branch_hint.h"
#include "frame.h"
#include "gpu_input.h"
#include "packets.h"
#include "tmds_encode.h"
#include "video_output.h"

uint32_t tmds_image_zero[FRAME_VIS_WIDTH / 2];

queue_t queue_unused;
queue_t queue_test;

extern void tmds_encode3_y1y2(const uint32_t* pixbuf, size_t length,
							  uint32_t* tmds);
extern void tmds_encode3_cbcr(const uint32_t* pixbuf, size_t length,
							  uint32_t* tmds);

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

	for(int x = 0; x < FRAME_VIS_WIDTH / 2; x++)
		tmds_image_zero[x] = 0xffd00;

	queue_init(&queue_unused, sizeof(struct tmds_data3*), 16);
	queue_init(&queue_test, sizeof(struct tmds_data3*), 16);

	for(int k = 0; k < queue_unused.element_count; k++) {
		struct tmds_data3* obj = malloc(sizeof(struct tmds_data3));
		obj->allocated = true;
		obj->vis_length = 0;
		obj->length = FRAME_VIS_WIDTH / 2;
		obj->ptr[0] = tmds_image_zero;
		obj->ptr[1] = malloc(obj->length * sizeof(uint32_t));
		obj->ptr[2] = malloc(obj->length * sizeof(uint32_t));

		for(size_t i = 0; i < obj->length; i++) {
			obj->ptr[1][i] = tmds_symbols_10h[4];
			obj->ptr[2][i] = tmds_symbols_80h[4];
		}

		queue_add_blocking(&queue_unused, &obj);
	}

	tmds_encode_init();
	tmds_encode_setup();
	packets_init();
	video_output_init((uint[]) {18, 20, 26}, 16, 8, &queue_unused);

	multicore_launch_core1(thread2);

	sleep_ms(1000);

	video_output_start();

	while(1) {
		struct tmds_data3* pixels;
		queue_remove_blocking(&queue_test, &pixels);
		tmds_encode3_cbcr(pixels->ptr[2], pixels->vis_length, pixels->ptr[2]);
		video_output_submit(pixels);
	}

	return 0;
}

size_t min_n(size_t a, size_t b) {
	return a < b ? a : b;
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
	*width = min_n((video_end - video_start + 2) / 3 * 3, FRAME_VIS_WIDTH / 2);
	*data = current_data;
	return true;
}

void thread2() {
	tmds_encode_setup();

	// TODO: only 32 because of an overflow during following hsync scan
	gpu_input_init(32, FRAME_WIDTH / 2 * 2, 2);
	gpu_input_start();

	for(int k = 0; k < FRAME_HEIGHT / 3 * 60 * 5; k++) {
		struct gpu_data* in = gpu_input_receive();
		gpu_input_release(in);
	}

	size_t current_idx, video_width;
	struct gpu_data* current_data;
	find_image_start(&current_data, &current_idx, &video_width);

	bool prev_blank = false;

	while(1) {
		bool blanking = (current_data->ptr[current_idx] & 0xFF00FF00) == 0;

		if(prev_blank && !blanking)
			break;

		prev_blank = blanking;
		current_idx += FRAME_WIDTH / 2;

		while(current_idx >= current_data->length) {
			current_idx -= current_data->length;
			gpu_input_release(current_data);
			current_data = gpu_input_receive();
		}
	}

	while(1) {
		for(int k = 0; k < FRAME_VIS_HEIGHT; k++) {
			struct tmds_data3* obj;
			queue_remove_blocking(&queue_unused, &obj);
			obj->vsync = k == 0;
			obj->vis_length = video_width;

			size_t line_idx = 0;

			while(line_idx < video_width) {
				size_t can_take = min_n(video_width - line_idx,
										current_data->length - current_idx);
				memcpy(obj->ptr[2] + line_idx, current_data->ptr + current_idx,
					   can_take * sizeof(uint32_t));

				line_idx += can_take;
				current_idx += can_take;

				while(current_idx >= current_data->length) {
					current_idx -= current_data->length;
					gpu_input_release(current_data);
					current_data = gpu_input_receive();
				}
			}

			tmds_encode3_y1y2(obj->ptr[2], obj->vis_length, obj->ptr[1]);
			queue_add_blocking(&queue_test, &obj);

			current_idx += FRAME_WIDTH / 2 - video_width;

			while(current_idx >= current_data->length) {
				current_idx -= current_data->length;
				gpu_input_release(current_data);
				current_data = gpu_input_receive();
			}
		}

		current_idx += FRAME_V_BLANK * FRAME_WIDTH / 2;

		while(current_idx >= current_data->length) {
			current_idx -= current_data->length;
			gpu_input_release(current_data);
			current_data = gpu_input_receive();
		}
	}
}