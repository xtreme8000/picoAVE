#include <stddef.h>

#include "frame.h"
#include "hardware/interp.h"
#include "tmds_encode.h"

#define READ_BIT(x, i) (((x) & (1 << (i))) ? 1 : 0)

uint32_t tmds_symbol0[256 * 9];
uint32_t tmds_symbol1[256 * 16];

// lsb is send first
uint32_t tmds_terc4_symbols2[256];
const uint16_t tmds_terc4_symbols[16] = {
	0x29c, 0x263, 0x2e4, 0x2e2, 0x171, 0x11e, 0x18e, 0x13c,
	0x2cc, 0x139, 0x19c, 0x2c6, 0x28e, 0x271, 0x163, 0x2c3,
};

// (vsync, hsync)
const uint32_t tmds_sync_symbols[4] = {
	0xd5354,
	0x2acab,
	0x55154,
	0xaaeab,
};

const uint32_t tmds_symbols_10h[9] = {
	/*
		bias -8, error 8, (18, 18)
		bias -6, error 13, (18, 19)
		bias -4, error 4, (18, 16)
		bias -2, error 8, (18, 18)
		bias 0, error 0, (16, 16)
		bias 2, error 4, (18, 16)
		bias 4, error 8, (18, 18)
		bias 6, error 20, (18, 20)
		bias 8, error 32, (20, 20)
		bias is 0 afterwards
	*/
	0xfc7f1, 0x7c7f1, 0x7c3f1, 0x43bf1, 0x7c1f0,
	0x7c10e, 0x4390e, 0x4310e, 0x4310c,
};

const uint32_t tmds_symbols_80h[9] = {
	/*
		bias -8, error 2, (127, 127)
		bias -6, error 4, (128, 130)
		bias -4, error 1, (128, 129)
		bias -2, error 0, (128, 128)
		bias 0, error 1, (129, 128)
		bias 2, error 1, (128, 127)
		bias 4, error 5, (126, 127)
		bias 6, error 5, (129, 130)
		bias 8, error 2, (129, 129)
		bias is 0 afterwards
	*/
	0x1fc7f, 0xe077f, 0xe037f, 0x6037f, 0x6017f,
	0x1fd80, 0x1fc80, 0xe0780, 0xe0380,
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

	for(size_t k = 0; k < 16; k++) {
		for(size_t i = 0; i < 16; i++) {
			tmds_terc4_symbols2[(k << 4) | i]
				= (tmds_terc4_symbols[k] << 10) | tmds_terc4_symbols[i];
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

void tmds_encode_sync(bool vsync, uint32_t* tmds0, uint32_t* tmds1,
					  uint32_t* tmds2) {
	static_assert((FRAME_WIDTH % 2) == 0);

	for(size_t k = 0; k < FRAME_WIDTH / 2; k++) {
		bool hsync = k >= FRAME_H_PORCH_FRONT / 2
			&& k < (FRAME_H_PORCH_FRONT + FRAME_H_SYNC) / 2;

		tmds0[k] = tmds_sync_lookup(vsync, !hsync);
		tmds1[k] = tmds_sync_symbols[0]; // CTL0 = 0, CTL1 = 0
		tmds2[k] = tmds_sync_symbols[0]; // CTL2 = 0, CTL3 = 0
	}
}

void tmds_encode_sync_video(uint32_t* tmds0, uint32_t* tmds1, uint32_t* tmds2) {
	static_assert((FRAME_H_BLANK % 2) == 0);

	for(size_t k = 0; k < FRAME_H_BLANK / 2; k++) {
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
	}
}

void tmds_encode_y1y2(const uint32_t* pixbuf, size_t length, uint32_t* tmds) {
	uint32_t bias = 0;

	const uint32_t* end = pixbuf + length;

	while(pixbuf != end) {
		uint32_t ycbcr;
		asm volatile("ldmia %0!, {%1}" : "+l"(pixbuf), "=l"(ycbcr));

		interp_set_accumulator(interp0, 0, ycbcr);

		// lut index: pixel [9:2] in bits, bias [13:10]
		// 0xF00003FF
		uint32_t symbol1;
		asm volatile("ldr %0, [%1, %2]"
					 : "=l"(symbol1)
					 : "l"(interp_peek_lane_result(interp0, 0)), "l"(bias));
		bias = symbol1 >> 26;

		// lut index: pixel [13:6] in bits, bias [5:2]
		// 0xF00FFC00
		uint32_t symbol2;
		asm volatile("ldr %0, [%1, %2]"
					 : "=l"(symbol2)
					 : "l"(interp_peek_lane_result(interp0, 1)), "l"(bias));
		bias = symbol2 >> 18;

		asm volatile("stmia %0!, {%1}"
					 : "+l"(tmds)
					 : "l"(symbol1 | symbol2)
					 : "memory");
	}
}

void tmds_encode_cbcr(const uint32_t* pixbuf, size_t length, uint32_t* tmds) {
	uint32_t bias = 0;

	const uint32_t* end = pixbuf + length;

	while(pixbuf != end) {
		uint32_t ycbcr;
		asm volatile("ldmia %0!, {%1}" : "+l"(pixbuf), "=l"(ycbcr));

		interp_set_accumulator(interp1, 0, ycbcr << 8);

		// lut index: pixel [9:2] in bits, bias [13:10]
		// 0xF00003FF
		uint32_t symbol1;
		asm volatile("ldr %0, [%1, %2]"
					 : "=l"(symbol1)
					 : "l"(interp_peek_lane_result(interp1, 0)), "l"(bias));
		bias = symbol1 >> 26;

		// lut index: pixel [13:6] in bits, bias [5:2]
		// 0xF00FFC00
		uint32_t symbol2;
		asm volatile("ldr %0, [%1, %2]"
					 : "=l"(symbol2)
					 : "l"(interp_peek_lane_result(interp1, 1)), "l"(bias));
		bias = symbol2 >> 18;

		asm volatile("stmia %0!, {%1}"
					 : "+l"(tmds)
					 : "l"(symbol1 | symbol2)
					 : "memory");
	}
}

void tmds_encode3_y1y2(const uint32_t* pixbuf, size_t length, uint32_t* tmds) {
	uint32_t bias = 0;
	uint32_t interp = (uint32_t)interp0;
	const uint32_t* end = pixbuf + length;

	while(pixbuf != end) {
		asm volatile("ldmia      %[pixbuf]!,{%%r4,%%r5,%%r6}\n\t"

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
					 : [bias] "+l"(bias), [pixbuf] "+l"(pixbuf),
					   [tmds] "+l"(tmds), [interp] "+l"(interp)
					 :
					 : "r4", "r5", "r6", "r7", "memory");
	}
}

// slower than y1y2
void tmds_encode3_cbcr(const uint32_t* pixbuf, size_t length, uint32_t* tmds) {
	uint32_t bias = 0;
	uint32_t interp = (uint32_t)interp1;
	const uint32_t* end = pixbuf + length;

	while(pixbuf != end) {
		asm volatile("ldmia      %[pixbuf]!,{%%r4,%%r5,%%r6}\n\t"

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
					 : [bias] "+l"(bias), [pixbuf] "+l"(pixbuf),
					   [tmds] "+l"(tmds), [interp] "+l"(interp)
					 :
					 : "r4", "r5", "r6", "r7", "memory");
	}
}

void tmds_encode_video(const uint32_t* pixbuf, bool cbcr, size_t length,
					   uint32_t* tmds) {
	size_t unrolled = length / 3 * 3;

	// TODO: keep bias between calls

	if(cbcr) {
		tmds_encode3_cbcr(pixbuf, unrolled, tmds);
		tmds_encode_cbcr(pixbuf + unrolled, length - unrolled, tmds + unrolled);
	} else {
		tmds_encode3_y1y2(pixbuf, unrolled, tmds);
		tmds_encode_y1y2(pixbuf + unrolled, length - unrolled, tmds + unrolled);
	}
}
