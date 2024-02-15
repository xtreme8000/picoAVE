#ifndef TMDS_SERIALIZER_H
#define TMDS_SERIALIZER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ffifo.h"
#include "pico/platform.h"

#define TMDS_QUEUE_LENGTH 8

struct tmds_data {
	uint32_t* ptr;
	size_t length;
};

FIFO_DEF(ffifo, struct tmds_data)

struct tmds_serializer {
	uint dma_channels[2];
	struct ffifo queue_send;
	uint pio_sm;
	int pending_transfers;
};

void tmds_serializer_init(void);
void tmds_serializer_create(struct tmds_serializer* s, uint gpio_base);
void tmds_serializer_wait_ready(struct tmds_serializer* s);
bool tmds_serializer_transfer_callback(struct tmds_serializer* s);

#endif