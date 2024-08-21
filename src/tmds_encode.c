/*
	Copyright (c) 2024 xtreme8000

	This file is part of picoAVE.

	picoAVE is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	picoAVE is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with picoAVE.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stddef.h>

#include "frame.h"
#include "hardware/interp.h"
#include "tmds_encode.h"
#include "utils.h"

static uint32_t tmds_symbol0[256 * 9];
static uint32_t tmds_symbol1[256 * 16];

// lsb is send first
uint16_t tmds_terc4_symbols[16] = {
	0x29c, 0x263, 0x2e4, 0x2e2, 0x171, 0x11e, 0x18e, 0x13c,
	0x2cc, 0x139, 0x19c, 0x2c6, 0x28e, 0x271, 0x163, 0x2c3,
};

// (vsync, hsync)
uint32_t tmds_sync_symbols[4] = {
	0xd5354,
	0x2acab,
	0x55154,
	0xaaeab,
};

uint32_t tmds_symbols_cbcr[9] = {
	/*
		bias -8, error 1.2047, (127,127)
		bias -6, error 2.2162, (122,129)
		bias -4, error 1.5960, (128,129)
		bias -2, error 0.0000, (128,128)
		bias 0, error 0.7835, (126,128)
		bias 2, error 0.8130, (128,127)
		bias 4, error 1.5965, (126,127)
		bias 6, error 2.2162, (122,129)
		bias 8, error 1.7652, (124,129)
		bias is 0 afterwards
	*/
	0x1fc7f, 0x5fc7c, 0xe037f, 0x6037f, 0x6027f,
	0x1fd80, 0x1fc80, 0xe0283, 0xe0281,
};

uint32_t tmds_sync_lookup(bool vsync, bool hsync) {
	return tmds_sync_symbols[(vsync << 1) | hsync];
}

static uint16_t symbol_encode(uint8_t b) {
	size_t ones = 0;

	for(size_t k = 0; k < 8; k++)
		ones += READ_BIT(b, k);

	uint16_t res = READ_BIT(b, 0);

	if(ones > 4 || (ones == 4 && !READ_BIT(b, 0))) {
		for(size_t k = 0; k <= 6; k++)
			res |= (!(READ_BIT(b, k + 1) ^ READ_BIT(res, k))) << (k + 1);
	} else {
		for(size_t k = 0; k <= 6; k++)
			res |= (READ_BIT(b, k + 1) ^ READ_BIT(res, k)) << (k + 1);

		res |= (1 << 8);
	}

	return res;
}

static int symbol_disparity(uint16_t b) {
	int ones = 0;

	for(size_t k = 0; k < 8; k++)
		ones += READ_BIT(b, k);

	return ones - (8 - ones);
}

static int symbol_dc_bias(int bias, uint16_t word, uint16_t* tmds_symbol) {
	int disparity = symbol_disparity(word);

	if(bias == 0 || disparity == 0) {
		if(READ_BIT(word, 8)) {
			*tmds_symbol = (1 << 8) | (word & 0xFF);
			return bias + disparity;
		} else {
			*tmds_symbol = (1 << 9) | (~word & 0xFF);
			return bias - disparity;
		}
	} else if((bias > 0 && disparity > 0) || (bias < 0 && disparity < 0)) {
		*tmds_symbol = (1 << 9) | (READ_BIT(word, 8) << 8) | (~word & 0xFF);
		return bias + 2 * READ_BIT(word, 8) - disparity;
	} else {
		*tmds_symbol = (0 << 9) | (READ_BIT(word, 8) << 8) | (word & 0xFF);
		return bias + disparity - 2 * READ_BIT(~word, 8);
	}
}

void tmds_encode_init() {
	for(int col = 0; col < 256; col++) {
		// {-8, -6, -4, -2, 0, 2, 4, 6, 8}
		for(int prev = 0; prev < 9; prev++) {
			uint16_t symbol;
			int next
				= (symbol_dc_bias(prev * 2 - 8, symbol_encode(col), &symbol)
				   + 8)
				/ 2;

			tmds_symbol0[(prev << 8) | col] = (next << 28) | (symbol << 10);
			tmds_symbol1[(col << 4) | prev] = (next << 28) | symbol;
		}
	}
}

void tmds_encode_setup() {
	interp_config c = interp_default_config();
	interp_config_set_signed(&c, false);

	// 0x0000FF00 -> 0x000003FC
	interp_config_set_cross_input(&c, false);
	interp_config_set_shift(&c, 6);
	interp_config_set_mask(&c, 2, 9);
	interp_set_base(interp0, 0, (uint32_t)tmds_symbol0);
	interp_set_config(interp0, 0, &c);

	// 0xFF000000 -> 0x00003FC0
	interp_config_set_cross_input(&c, true);
	interp_config_set_shift(&c, 18);
	interp_config_set_mask(&c, 6, 13);
	interp_set_base(interp0, 1, (uint32_t)tmds_symbol1);
	interp_set_config(interp0, 1, &c);

	// interp1 has both pixels flipped
	// 0xFF000000 -> 0x000003FC
	interp_config_set_cross_input(&c, false);
	interp_config_set_shift(&c, 22);
	interp_config_set_mask(&c, 2, 9);
	interp_set_base(interp1, 0, (uint32_t)tmds_symbol0);
	interp_set_config(interp1, 0, &c);

	// 0x0000FF00 -> 0x00003FC0
	interp_config_set_cross_input(&c, true);
	interp_config_set_shift(&c, 2);
	interp_config_set_mask(&c, 6, 13);
	interp_set_base(interp1, 1, (uint32_t)tmds_symbol1);
	interp_set_config(interp1, 1, &c);
}

void tmds_encode_sync(size_t length, bool vsync, uint32_t* tmds0,
					  uint32_t* tmds1, uint32_t* tmds2) {
	assert((length % 2) == 0);

	for(size_t k = 0; k < length / 2; k++) {
		bool hsync = k >= FRAME_H_PORCH_FRONT / 2
			&& k < (FRAME_H_PORCH_FRONT + FRAME_H_SYNC) / 2;

		tmds0[k] = tmds_sync_lookup(vsync, !hsync);
		tmds1[k] = tmds_sync_symbols[0]; // CTL0 = 0, CTL1 = 0
		tmds2[k] = tmds_sync_symbols[0]; // CTL2 = 0, CTL3 = 0
	}
}

void tmds_encode_sync_video(uint32_t* tmds0, uint32_t* tmds1, uint32_t* tmds2) {
	static_assert((FRAME_H_BLANK % 2) == 0);

	/*for(size_t k = 0; k < FRAME_H_BLANK / 2; k++) {
		bool hsync = k >= FRAME_H_PORCH_FRONT / 2
			&& k < (FRAME_H_PORCH_FRONT + FRAME_H_SYNC) / 2;

		if(k < (FRAME_H_BLANK - VIDEO_DATA_PREAMBLE_LENGTH - 2) / 2) {
			tmds0[k] = tmds_sync_lookup(true, !hsync);
			tmds1[k] = tmds_sync_symbols[0]; // CTL0 = 0, CTL1 = 0
			tmds2[k] = tmds_sync_symbols[0]; // CTL2 = 0, CTL3 = 0
		} else if(k == FRAME_H_BLANK / 2 - 1) {
			tmds0[k] = VIDEO_DATA_GUARD_BAND_0;
			tmds1[k] = VIDEO_DATA_GUARD_BAND_1;
			tmds2[k] = VIDEO_DATA_GUARD_BAND_2;
		} else {
			tmds0[k] = tmds_sync_lookup(true, !hsync);
			tmds1[k] = tmds_sync_symbols[1]; // CTL0 = 1, CTL1 = 0
			tmds2[k] = tmds_sync_symbols[0]; // CTL2 = 0, CTL3 = 0
		}
	}*/

#ifndef DVI_ONLY
	for(size_t k = 0; k < VIDEO_DATA_PREAMBLE_LENGTH / 2; k++) {
		tmds0[k] = tmds_sync_lookup(true, true);
		tmds1[k] = tmds_sync_symbols[1]; // CTL0 = 1, CTL1 = 0
		tmds2[k] = tmds_sync_symbols[0]; // CTL2 = 0, CTL3 = 0
	}

	tmds0[VIDEO_DATA_PREAMBLE_LENGTH / 2] = VIDEO_DATA_GUARD_BAND_0;
	tmds1[VIDEO_DATA_PREAMBLE_LENGTH / 2] = VIDEO_DATA_GUARD_BAND_1;
	tmds2[VIDEO_DATA_PREAMBLE_LENGTH / 2] = VIDEO_DATA_GUARD_BAND_2;
#else
	for(size_t k = 0; k <= VIDEO_DATA_PREAMBLE_LENGTH / 2; k++) {
		tmds0[k] = tmds_sync_lookup(true, true);
		tmds1[k] = tmds_sync_symbols[0]; // CTL0 = 0, CTL1 = 0
		tmds2[k] = tmds_sync_symbols[0]; // CTL2 = 0, CTL3 = 0
	}
#endif
}

uint32_t tmds_encode_y1y2(const uint32_t* pixbuf, size_t length,
						  uint32_t* tmds) {
	uint32_t bias = 0;

	asm volatile("loop%=:\n\t"
				 "ldmia      %[pixbuf]!,{%%r4,%%r5,%%r6}\n\t"

				 "str        %%r4,[%[interp],#0x00]\n\t" // accum 0

				 "ldr        %%r4,[%[interp],#0x20]\n\t" // lane0 peek
				 "ldr        %%r4,[%%r4,%[bias]]\n\t"
				 "lsr        %[bias],%%r4,#26\n\t"

				 "ldr        %%r7,[%[interp],#0x24]\n\t" // lane1 peek
				 "ldr        %%r7,[%%r7,%[bias]]\n\t"
				 "lsr        %[bias],%%r7,#18\n\t"
				 "orr        %%r4,%%r7\n\t"

				 "str        %%r5,[%[interp],#0x00]\n\t"

				 "ldr        %%r5,[%[interp],#0x20]\n\t"
				 "ldr        %%r5,[%%r5,%[bias]]\n\t"
				 "lsr        %[bias],%%r5,#26\n\t"

				 "ldr        %%r7,[%[interp],#0x24]\n\t"
				 "ldr        %%r7,[%%r7,%[bias]]\n\t"
				 "lsr        %[bias],%%r7,#18\n\t"
				 "orr        %%r5,%%r7\n\t"

				 "str        %%r6,[%[interp],#0x00]\n\t"

				 "ldr        %%r6,[%[interp],#0x20]\n\t"
				 "ldr        %%r6,[%%r6,%[bias]]\n\t"
				 "lsr        %[bias],%%r6,#26\n\t"

				 "ldr        %%r7,[%[interp],#0x24]\n\t"
				 "ldr        %%r7,[%%r7,%[bias]]\n\t"
				 "lsr        %[bias],%%r7,#18\n\t"
				 "orr        %%r6,%%r7\n\t"

				 "stmia      %[tmds]!,{%%r4,%%r5,%%r6}\n\t"
				 "cmp        %[end],%[pixbuf]\n\t"
				 "bhi        loop%=\n\t"
				 : [bias] "+l"(bias), [pixbuf] "+l"(pixbuf), [tmds] "+l"(tmds)
				 : [interp] "l"((uint32_t)interp0), [end] "h"(pixbuf + length)
				 : "r4", "r5", "r6", "r7", "cc", "memory");

	return bias >> 10;
}

// slower than y1y2
uint32_t tmds_encode_cbcr(const uint32_t* pixbuf, size_t length,
						  uint32_t* tmds) {
	uint32_t bias = 0;

	asm volatile("loop%=:\n\t"
				 "ldmia      %[pixbuf]!,{%%r4,%%r5,%%r6}\n\t"

				 "lsl        %%r4, #8\n\t"
				 "str        %%r4,[%[interp],#0x00]\n\t" // accum 0

				 "ldr        %%r4,[%[interp],#0x20]\n\t" // lane0 peek
				 "ldr        %%r4,[%%r4,%[bias]]\n\t"
				 "lsr        %[bias],%%r4,#26\n\t"

				 "ldr        %%r7,[%[interp],#0x24]\n\t" // lane1 peek
				 "ldr        %%r7,[%%r7,%[bias]]\n\t"
				 "lsr        %[bias],%%r7,#18\n\t"
				 "orr        %%r4,%%r7\n\t"

				 "lsl        %%r5, #8\n\t"
				 "str        %%r5,[%[interp],#0x00]\n\t"

				 "ldr        %%r5,[%[interp],#0x20]\n\t"
				 "ldr        %%r5,[%%r5,%[bias]]\n\t"
				 "lsr        %[bias],%%r5,#26\n\t"

				 "ldr        %%r7,[%[interp],#0x24]\n\t"
				 "ldr        %%r7,[%%r7,%[bias]]\n\t"
				 "lsr        %[bias],%%r7,#18\n\t"
				 "orr        %%r5,%%r7\n\t"

				 "lsl        %%r6, #8\n\t"
				 "str        %%r6,[%[interp],#0x00]\n\t"

				 "ldr        %%r6,[%[interp],#0x20]\n\t"
				 "ldr        %%r6,[%%r6,%[bias]]\n\t"
				 "lsr        %[bias],%%r6,#26\n\t"

				 "ldr        %%r7,[%[interp],#0x24]\n\t"
				 "ldr        %%r7,[%%r7,%[bias]]\n\t"
				 "lsr        %[bias],%%r7,#18\n\t"
				 "orr        %%r6,%%r7\n\t"

				 "stmia      %[tmds]!,{%%r4,%%r5,%%r6}\n\t"
				 "cmp        %[end],%[pixbuf]\n\t"
				 "bhi        loop%=\n\t"
				 : [bias] "+l"(bias), [pixbuf] "+l"(pixbuf), [tmds] "+l"(tmds)
				 : [interp] "l"((uint32_t)interp1), [end] "h"(pixbuf + length)
				 : "r4", "r5", "r6", "r7", "cc", "memory");

	return bias >> 10;
}
