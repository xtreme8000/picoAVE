#include <assert.h>

#include "clock.pio.h"
#include "tmds_clock.h"

static uint pio_program_offset;

void tmds_clock_init() {
	pio_program_offset = pio_add_program(pio0, &pio_clock_program);
}

void tmds_clock_create(struct tmds_clock* s, uint gpio_base) {
	assert(s && gpio_base < 32 && (gpio_base % 2) == 0);

	s->pio_sm = pio_claim_unused_sm(pio0, true);
	pio_clock_program_init(pio0, s->pio_sm, pio_program_offset, gpio_base);
}

void tmds_start_with_clock(struct tmds_clock* c, struct tmds_serializer* s,
						   size_t length) {
	assert(c && s && length > 0);
	uint32_t mask = 1 << c->pio_sm;

	for(size_t k = 0; k < length; k++)
		mask |= 1 << s[k].pio_sm;

	pio_enable_sm_mask_in_sync(pio0, mask);
}