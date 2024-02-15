#ifndef GPU_INPUT_H
#define GPU_INPUT_H

#include "ffifo.h"
#include "pico/platform.h"

struct gpu_data {
	size_t data_skipped;
	size_t length;
	uint32_t* ptr;
};

void gpu_input_init(size_t capacity, size_t buffer_length, uint gpio_base);
void gpu_input_start(void);
struct gpu_data* gpu_input_receive(void);
void gpu_input_release(struct gpu_data* d);

#endif