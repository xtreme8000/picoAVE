#ifndef TMDS_CLOCK_H
#define TMDS_CLOCK_H

#include "pico/platform.h"
#include "tmds_serializer.h"

struct tmds_clock {
	uint pio_sm;
};

void tmds_clock_init(void);
void tmds_clock_create(struct tmds_clock* s, uint gpio_base);
void tmds_start_with_clock(struct tmds_clock* c, struct tmds_serializer* s,
						   size_t length);

#endif