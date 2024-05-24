#include <stdlib.h>
#include <string.h>

#include "hardware/structs/bus_ctrl.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "font.h"
#include "frame.h"
#include "gpu_input.h"
#include "mem_pool.h"
#include "metadata.h"
#include "packets.h"
#include "str_builder.h"
#include "tmds_encode.h"
#include "utils.h"
#include "video_output.h"

uint32_t tmds_image_00h[FRAME_BUFFER_WIDTH / 2];
uint32_t tmds_image_10h[FRAME_BUFFER_WIDTH / 2];
uint32_t tmds_image_80h[FRAME_BUFFER_WIDTH / 2];

struct mem_pool pool_packets;
struct mem_pool pool_video;
struct mem_pool pool_audio;

queue_t queue_test;
queue_t queue_test_audio;

void thread1(void);
void thread2(void);
void encode_video_isr(void);

static void* alloc_video() {
	struct tmds_data3* obj = malloc(sizeof(struct tmds_data3));
	obj->type = TYPE_VIDEO;
	obj->encode_offset = 0;
	obj->encode_length = 0;
	obj->length = FRAME_BUFFER_WIDTH / 2;
	obj->ptr[0] = malloc(obj->length * sizeof(uint32_t));
	obj->ptr[1] = malloc(obj->length * sizeof(uint32_t));
	obj->ptr[2] = malloc(obj->length * sizeof(uint32_t));
	return obj;
}

static void* alloc_audio() {
	return malloc(sizeof(struct audio_data));
}

static void* alloc_packet() {
	struct tmds_data3* obj = malloc(sizeof(struct tmds_data3));
	obj->type = TYPE_PACKET;
	obj->length = (FRAME_WIDTH - FRAME_BUFFER_WIDTH) / 2;
	obj->ptr[0] = malloc(obj->length * sizeof(uint32_t));
	obj->ptr[1] = malloc(obj->length * sizeof(uint32_t));
	obj->ptr[2] = malloc(obj->length * sizeof(uint32_t));

	tmds_encode_sync(obj->length * 2, true, obj->ptr[0], obj->ptr[1],
					 obj->ptr[2]);
	return obj;
}

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

	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

	for(size_t k = 0; k < FRAME_BUFFER_WIDTH / 2; k++) {
		tmds_image_00h[k] = 0xffd00;
		tmds_image_10h[k] = tmds_symbols_10h[4];
		tmds_image_80h[k] = tmds_symbols_80h[4];
	}

	tmds_encode_sync_video(tmds_image_00h, tmds_image_10h, tmds_image_80h);

	mem_pool_create(&pool_video, alloc_video, 16, NULL);
	mem_pool_create(&pool_audio, alloc_audio, 8, NULL);
	mem_pool_create(&pool_packets, alloc_packet, 16, NULL);
	queue_init(&queue_test, sizeof(struct tmds_data3*),
			   mem_pool_capacity(&pool_video));
	queue_init(&queue_test_audio, sizeof(struct audio_data*),
			   mem_pool_capacity(&pool_audio));

	tmds_encode_init();
	tmds_encode_setup();
	packets_init();
	video_output_init((uint[]) {18, 20, 26}, 16, &pool_video, &pool_packets);

	multicore_launch_core1(thread2);

	irq_set_exclusive_handler(SIO_IRQ_PROC0, encode_video_isr);
	irq_set_enabled(SIO_IRQ_PROC0, true);

	thread1();

	return 0;
}

static size_t cnt = 0;

void CORE0_CODE encode_video_isr() {
	multicore_fifo_drain();
	multicore_fifo_clear_irq();

	struct tmds_data3* obj;
	while(queue_try_remove(&queue_test, &obj)) {
		if(obj->encode_length > 0) {
			size_t bias = tmds_encode_y1y2(obj->ptr[1] + obj->encode_offset,
										   obj->encode_length,
										   obj->ptr[1] + obj->encode_offset);

			if(obj->encode_offset + obj->encode_length < obj->length)
				obj->ptr[1][obj->encode_offset + obj->encode_length]
					= tmds_symbols_10h[bias];
		}

		bool finished = obj->last_line;
		video_output_submit(obj);

		if(finished) {
			cnt++;
			if(cnt == 15) {
				cnt = 0;
				gpio_xor_mask(1 << PICO_DEFAULT_LED_PIN);
			}
		}
	}
}

void CORE0_CODE thread1() {
	video_output_start();

	while(1) {
		struct audio_data* frame;
		queue_remove_blocking(&queue_test_audio, &frame);

		for(size_t idx = 0; idx < AUDIO_FRAME_LENGTH; idx += 4) {
			struct tmds_data3* obj = mem_pool_alloc(&pool_packets);

			packets_encode_audio(frame->audio_data + idx, idx, false, true,
								 obj->ptr[0] + FRAME_H_PORCH_FRONT / 2,
								 obj->ptr[1] + FRAME_H_PORCH_FRONT / 2,
								 obj->ptr[2] + FRAME_H_PORCH_FRONT / 2);

			video_output_submit(obj);
		}

		mem_pool_free(&pool_audio, frame);
	}
}

#define BLANK_MASK 0xF000F000 // check any Y >= 16

bool CORE1_CODE advance_input(size_t* idx, struct gpu_data** data,
							  size_t amount) {
	*idx += amount;

	while(*idx >= (*data)->length) {
		*idx -= (*data)->length;
		gpu_input_release(*data);

		struct gpu_data* res = gpu_input_receive();
		*data = res;

		if(res->data_skipped)
			return false;
	}

	return true;
}

bool CORE1_CODE find_image_start(struct gpu_data** data, size_t* start,
								 size_t* width) {
	struct gpu_data* current_data = gpu_input_receive();

	size_t video_start, video_end;
	size_t k = 0;

	while(1) {
		bool blanking = (current_data->ptr[k] & BLANK_MASK) == 0;

		if(unlikely(blanking))
			break;

		if(unlikely((++k) >= current_data->length)) {
			gpu_input_release(current_data);
			return false;
		}
	}

	while(1) {
		bool blanking = (current_data->ptr[k] & BLANK_MASK) == 0;

		if(unlikely(!blanking)) {
			video_start = k;
			break;
		}

		if(unlikely((++k) >= current_data->length)) {
			gpu_input_release(current_data);
			return false;
		}
	}

	while(1) {
		bool blanking = (current_data->ptr[k] & BLANK_MASK) == 0;

		if(unlikely(blanking)) {
			video_end = k;
			break;
		}

		if(unlikely((++k) >= current_data->length)) {
			gpu_input_release(current_data);
			return false;
		}
	}

	*start = video_start;
	*width = min_n(video_end - video_start, FRAME_VIS_WIDTH / 2);
	*data = current_data;
	return true;
}

struct gpu_sync_state {
	size_t video_xstart;
	size_t video_vstart, video_vend;
	size_t video_width, video_width_padded;
	struct gpu_data* current_data;
	size_t current_idx;
	char msg[32];
	int msg_frames_visible;
};

bool CORE1_CODE gpu_sync_video(struct gpu_sync_state* state) {
	size_t video_objs_length = mem_pool_capacity(&pool_video);
	struct tmds_data3* video_objs[video_objs_length];

	for(size_t k = 0; k < video_objs_length; k++) {
		video_objs[k] = mem_pool_alloc(&pool_video);
		memcpy(video_objs[k]->ptr[0], tmds_image_00h, sizeof(tmds_image_00h));
		memcpy(video_objs[k]->ptr[1], tmds_image_10h, sizeof(tmds_image_10h));
		memcpy(video_objs[k]->ptr[2], tmds_image_80h, sizeof(tmds_image_80h));
	}

	for(size_t k = 0; k < video_objs_length; k++)
		mem_pool_free(&pool_video, video_objs[k]);

	size_t current_idx, video_width;
	struct gpu_data* current_data;
	while(1) {
		gpu_input_drain();

		if(find_image_start(&current_data, &current_idx, &video_width))
			break;
	}

	bool success = true;
	bool prev_blank = false;
	size_t loop_counter = 0;

	while(1) {
		bool blanking = (current_data->ptr[current_idx] & BLANK_MASK) == 0;

		if(prev_blank && !blanking)
			break;

		prev_blank = blanking;

		if(!advance_input(&current_idx, &current_data, FRAME_WIDTH / 2)
		   || loop_counter >= FRAME_HEIGHT * 4) {
			success = false;
			break;
		}

		loop_counter++;
	}

	size_t video_height = 0;

	while(video_height < FRAME_VIS_HEIGHT) {
		bool blanking = (current_data->ptr[current_idx] & BLANK_MASK) == 0;

		if(blanking)
			break;

		if(!advance_input(&current_idx, &current_data, FRAME_WIDTH / 2)) {
			success = false;
			break;
		}

		video_height++;
	}

	if(!advance_input(&current_idx, &current_data,
					  (FRAME_HEIGHT - video_height) * FRAME_WIDTH / 2))
		success = false;

	static_assert(FRAME_VIS_WIDTH / 2 % 3 == 0);
	size_t video_width_padded = (video_width + 2) / 3 * 3;
	size_t video_xstart = (FRAME_VIS_WIDTH / 2 - video_width) / 2;

	if(video_xstart + video_width_padded > FRAME_VIS_WIDTH / 2)
		video_xstart = 0;

	size_t video_vstart = (FRAME_VIS_HEIGHT - video_height) / 2;
	size_t video_vend = video_height + video_vstart;

	state->video_xstart = video_xstart;
	state->video_vstart = video_vstart;
	state->video_vend = video_vend;
	state->video_width = video_width;
	state->video_width_padded = video_width_padded;
	state->current_data = current_data;
	state->current_idx = current_idx;

	str_finish(str_append(
		str_uint(str_char(str_uint(state->msg, video_width * 2), 'x'),
				 video_height),
		"p [" PROJECT_NAME " " PROJECT_VERSION "]"));
	state->msg_frames_visible = 60 * 10;

	return success;
}

struct tmds_data3 empty_line;
struct tmds_data3 empty_line_sync;
struct tmds_data3 empty_line_last;

void CORE1_CODE thread2() {
	empty_line = empty_line_sync = empty_line_last = (struct tmds_data3) {
		.type = TYPE_CONST,
		.encode_length = 0,
		.length = FRAME_BUFFER_WIDTH / 2,
		.ptr = {tmds_image_00h, tmds_image_10h, tmds_image_80h},
	};

	empty_line.vsync = false;
	empty_line.last_line = false;

	empty_line_sync.vsync = true;
	empty_line_sync.last_line = false;

	empty_line_last.vsync = false;
	empty_line_last.last_line = true;

	tmds_encode_setup();

	// TODO: only 32 because of an overflow during following sync scan
	gpu_input_init(15, FRAME_WIDTH / 2 * 4, 2, &pool_audio, &queue_test_audio,
				   12, 11);
	gpu_input_start();

	struct gpu_sync_state gpu_sync;
	int needs_resync_frames = 0;

	while(!gpu_sync_video(&gpu_sync))
		tight_loop_contents();

	while(1) {
		bool needs_resync = false;

		for(size_t k = gpu_sync.video_vstart; k < gpu_sync.video_vend; k++) {
			struct tmds_data3* obj = mem_pool_alloc(&pool_video);
			obj->vsync = k == 0;
			obj->last_line = k == FRAME_VIS_HEIGHT - 1;
			obj->encode_offset
				= FRAME_BUFFER_OFFSET / 2 + gpu_sync.video_xstart;
			obj->encode_length = gpu_sync.video_width_padded;

			bool blanking1 = (gpu_sync.current_data->ptr[gpu_sync.current_idx]
							  & BLANK_MASK)
				== 0;
			if(blanking1)
				needs_resync = true;

			size_t line_idx = 0;

			if(gpu_sync.msg_frames_visible > 0
			   && k < gpu_sync.video_vstart + FONT_CHAR_HEIGHT) {
				size_t width
					= font_encode(gpu_sync.msg, k - gpu_sync.video_vstart,
								  obj->ptr[1] + obj->encode_offset);

				line_idx += width;
				if(!advance_input(&gpu_sync.current_idx, &gpu_sync.current_data,
								  width))
					needs_resync = true;
			}

			while(line_idx < gpu_sync.video_width) {
				size_t can_take = min_n(gpu_sync.video_width - line_idx,
										gpu_sync.current_data->length
											- gpu_sync.current_idx);
				memcpy(obj->ptr[1] + obj->encode_offset + line_idx,
					   gpu_sync.current_data->ptr + gpu_sync.current_idx,
					   can_take * sizeof(uint32_t));

				line_idx += can_take;
				if(!advance_input(&gpu_sync.current_idx, &gpu_sync.current_data,
								  can_take))
					needs_resync = true;
			}

			while(line_idx < gpu_sync.video_width_padded)
				obj->ptr[1][obj->encode_offset + line_idx++] = 0x10801080;

			size_t bias = tmds_encode_cbcr(obj->ptr[1] + obj->encode_offset,
										   gpu_sync.video_width_padded,
										   obj->ptr[2] + obj->encode_offset);

			if(obj->encode_offset + gpu_sync.video_width_padded < obj->length)
				obj->ptr[2][obj->encode_offset + gpu_sync.video_width_padded]
					= tmds_symbols_80h[bias];

			queue_add_blocking(&queue_test, &obj);
			multicore_fifo_push_blocking(0);

			bool blanking2 = (gpu_sync.current_data->ptr[gpu_sync.current_idx]
							  & BLANK_MASK)
				== 0;
			if(!blanking2)
				needs_resync = true;

			if(!advance_input(&gpu_sync.current_idx, &gpu_sync.current_data,
							  FRAME_WIDTH / 2 - gpu_sync.video_width))
				needs_resync = true;
		}

		for(size_t k = gpu_sync.video_vend; k < FRAME_VIS_HEIGHT; k++) {
			struct tmds_data3* obj
				= (k == FRAME_VIS_HEIGHT - 1) ? &empty_line_last : &empty_line;
			queue_add_blocking(&queue_test, &obj);
			multicore_fifo_push_blocking(0);

			bool blanking = (gpu_sync.current_data->ptr[gpu_sync.current_idx]
							 & BLANK_MASK)
				== 0;
			if(!blanking)
				needs_resync = true;

			if(!advance_input(&gpu_sync.current_idx, &gpu_sync.current_data,
							  FRAME_WIDTH / 2))
				needs_resync = true;
		}

		for(size_t k = 0; k < FRAME_V_BLANK; k++) {
			bool blanking = (gpu_sync.current_data->ptr[gpu_sync.current_idx]
							 & BLANK_MASK)
				== 0;
			if(!blanking)
				needs_resync = true;

			if(!advance_input(&gpu_sync.current_idx, &gpu_sync.current_data,
							  FRAME_WIDTH / 2))
				needs_resync = true;
		}

		for(size_t k = 0; k < gpu_sync.video_vstart; k++) {
			struct tmds_data3* obj = (k == 0) ? &empty_line_sync : &empty_line;
			queue_add_blocking(&queue_test, &obj);
			multicore_fifo_push_blocking(0);

			bool blanking = (gpu_sync.current_data->ptr[gpu_sync.current_idx]
							 & BLANK_MASK)
				== 0;
			if(!blanking)
				needs_resync = true;

			if(!advance_input(&gpu_sync.current_idx, &gpu_sync.current_data,
							  FRAME_WIDTH / 2))
				needs_resync = true;
		}

		if(gpu_sync.msg_frames_visible > 0)
			gpu_sync.msg_frames_visible--;

		if(needs_resync)
			needs_resync_frames++;

		if(needs_resync_frames >= 60) {
			needs_resync_frames = 0;
			gpu_input_release(gpu_sync.current_data);

			while(!gpu_sync_video(&gpu_sync))
				tight_loop_contents();
		}
	}
}