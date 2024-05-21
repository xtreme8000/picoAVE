#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "packets.h"
#include "pico/stdlib.h"
#include "tmds_encode.h"
#include "utils.h"

static uint32_t CORE0_DATA tmds_terc4_symbols2[256];
static uint8_t CORE0_DATA bch_lookup[256];
static uint32_t CORE0_DATA sub_lookup[256];
static uint8_t parity_lookup[256];
static uint32_t header_lookup[256];

static uint8_t bch_gen(uint8_t in) {
	uint8_t state = 0;

	for(size_t i = 0; i < 8; i++) {
		uint8_t feedback = (state ^ in) & 0x01;
		in >>= 1;

		uint8_t p0 = ((state & 0x02) ^ (feedback << 1)) >> 1;
		uint8_t p1 = ((state & 0x04) ^ (feedback << 2)) >> 1;
		uint8_t p2 = (state & 0xF8) >> 1;
		uint8_t p3 = feedback << 7;

		state = p0 | p1 | p2 | p3;
	}

	return state;
}

static uint8_t bch_calc(uint8_t* data, size_t length) {
	uint8_t state = 0;

	for(size_t k = 0; k < length; k++)
		state = bch_lookup[data[k] ^ state];

	return state;
}

void packets_init() {
	for(size_t k = 0; k < 256; k++) {
		bch_lookup[k] = bch_gen(k);
		parity_lookup[k] = __builtin_parity(k);

		header_lookup[k] = (READ_BIT(k, 7) << 30) | (READ_BIT(k, 6) << 26)
			| (READ_BIT(k, 5) << 22) | (READ_BIT(k, 4) << 18)
			| (READ_BIT(k, 3) << 14) | (READ_BIT(k, 2) << 10)
			| (READ_BIT(k, 1) << 6) | (READ_BIT(k, 0) << 2);

		sub_lookup[k] = (READ_BIT(k, 7) << 28) | (READ_BIT(k, 5) << 24)
			| (READ_BIT(k, 3) << 20) | (READ_BIT(k, 1) << 16)
			| (READ_BIT(k, 6) << 12) | (READ_BIT(k, 4) << 8)
			| (READ_BIT(k, 2) << 4) | READ_BIT(k, 0);
	}

	for(size_t k = 0; k < 16; k++) {
		for(size_t i = 0; i < 16; i++) {
			tmds_terc4_symbols2[(k << 4) | i]
				= (tmds_terc4_symbols[k] << 10) | tmds_terc4_symbols[i];
		}
	}
}

static void encode_header(uint8_t* data, uint32_t* tmds0, bool hsync,
						  bool vsync, bool first_packet) {
	size_t pos = 0;
	uint32_t sync32
		= __mul_instruction(0x11111111, 0x08 | (vsync << 1) | hsync);
	uint32_t sync32_f = sync32 & 0xFFFFFFF7;

	for(size_t k = 0; k < PACKET_HEADER_LENGTH; k++) {
		uint32_t sync = (k == 0 && first_packet) ? sync32_f : sync32;
		uint32_t res = header_lookup[data[k]] | sync;

		tmds0[pos++] = tmds_terc4_symbols2[res & 0xFF];
		tmds0[pos++] = tmds_terc4_symbols2[(res >> 8) & 0xFF];
		tmds0[pos++] = tmds_terc4_symbols2[(res >> 16) & 0xFF];
		tmds0[pos++] = tmds_terc4_symbols2[res >> 24];
	}
}

static void encode_sub_lane(uint8_t* data, uint32_t* tmds1, uint32_t* tmds2) {
	size_t pos = 0;

	for(size_t k = 0; k < PACKET_SUBLANE_LENGTH; k++) {
		uint32_t res = 0;

		for(size_t i = 0; i < 4; i++)
			res |= sub_lookup[data[k + i * 8]] << i;

		tmds1[pos + 0] = tmds_terc4_symbols2[res & 0xFF];
		tmds1[pos + 1] = tmds_terc4_symbols2[(res >> 8) & 0xFF];
		tmds2[pos + 0] = tmds_terc4_symbols2[(res >> 16) & 0xFF];
		tmds2[pos + 1] = tmds_terc4_symbols2[res >> 24];
		pos += 2;
	}
}

static void data_encode(struct packet* p, bool hsync, bool vsync,
						bool first_packet, uint32_t* tmds0, uint32_t* tmds1,
						uint32_t* tmds2) {
	p->header[PACKET_HEADER_LENGTH - 1]
		= bch_calc(p->header, PACKET_HEADER_LENGTH - 1);

	for(size_t k = 0; k < PACKET_SUBLANE_COUNT; k++)
		p->data[(k + 1) * PACKET_SUBLANE_LENGTH - 1] = bch_calc(
			p->data + k * PACKET_SUBLANE_LENGTH, PACKET_SUBLANE_LENGTH - 1);

	encode_header(p->header, tmds0, hsync, vsync, first_packet);
	encode_sub_lane(p->data, tmds1, tmds2);
}

// encode packets with constant h+vsync
void packets_encode(struct packet* p, size_t amount, bool hsync, bool vsync,
					uint32_t* tmds0, uint32_t* tmds1, uint32_t* tmds2) {
	// preamble (8 pixels)
	for(size_t k = 0; k < DATA_ISLAND_PREAMBLE_LENGTH / 2; k++) {
		*(tmds0++) = tmds_sync_lookup(vsync, hsync);
		*(tmds1++) = tmds_sync_symbols[1]; // CTL0 = 1, CTL1 = 0
		*(tmds2++) = tmds_sync_symbols[1]; // CTL2 = 1, CTL3 = 0
	}

	size_t sync_state = 0x0C | (vsync << 1) | hsync;
	uint32_t terc4_sync = tmds_terc4_symbols2[(sync_state << 4) | sync_state];

	// leading guard band (2 pixels)
	*(tmds0++) = terc4_sync;
	*(tmds1++) = DATA_ISLAND_GUARD_BAND;
	*(tmds2++) = DATA_ISLAND_GUARD_BAND;

	// data island (32 pixels * amount)
	for(size_t k = 0; k < amount; k++) {
		data_encode(p + k, hsync, vsync, k == 0, tmds0, tmds1, tmds2);
		tmds0 += PACKET_BODY_LENGTH / 2;
		tmds1 += PACKET_BODY_LENGTH / 2;
		tmds2 += PACKET_BODY_LENGTH / 2;
	}

	// trailing guard band (2 pixels)
	*(tmds0++) = terc4_sync;
	*(tmds1++) = DATA_ISLAND_GUARD_BAND;
	*(tmds2++) = DATA_ISLAND_GUARD_BAND;
}

// length of encoded packets in pixels
size_t packets_length(size_t amount) {
	return amount * PACKET_BODY_LENGTH + 4 + DATA_ISLAND_PREAMBLE_LENGTH;
}

// encode audio with constant h+vsync
void packets_encode_audio(uint32_t samples[4], size_t frame, bool hsync,
						  bool vsync, uint32_t* tmds0, uint32_t* tmds1,
						  uint32_t* tmds2) {
	// preamble (8 pixels)
	for(size_t k = 0; k < DATA_ISLAND_PREAMBLE_LENGTH / 2; k++) {
		*(tmds0++) = tmds_sync_lookup(vsync, hsync);
		*(tmds1++) = tmds_sync_symbols[1]; // CTL0 = 1, CTL1 = 0
		*(tmds2++) = tmds_sync_symbols[1]; // CTL2 = 1, CTL3 = 0
	}

	size_t sync_state = 0x0C | (vsync << 1) | hsync;
	uint32_t terc4_sync = tmds_terc4_symbols2[(sync_state << 4) | sync_state];

	// leading guard band (2 pixels)
	*(tmds0++) = terc4_sync;
	*(tmds1++) = DATA_ISLAND_GUARD_BAND;
	*(tmds2++) = DATA_ISLAND_GUARD_BAND;

	struct packet p;
	p.header[0] = 0x02;
	p.header[1] = 0x0F;
	p.header[2] = (frame == 0) ? 0x10 : 0x00;

	for(size_t k = 0; k < PACKET_BODY_LENGTH; k += PACKET_SUBLANE_LENGTH) {
		uint32_t s = *(samples++);

		// left channel
		p.data[k + 0] = 0;
		p.data[k + 1] = (s >> 16) & 0xFF;
		p.data[k + 2] = s >> 24;

		// right channel
		p.data[k + 3] = 0;
		p.data[k + 4] = s & 0xFF;
		p.data[k + 5] = (s >> 8) & 0xFF;

		uint8_t p_left = parity_lookup[p.data[k + 1] ^ p.data[k + 2] ^ 0x01];
		uint8_t p_right = parity_lookup[p.data[k + 4] ^ p.data[k + 5] ^ 0x01];

		p.data[k + 6] = (p_right << 7) | (p_left << 3) | 0x11;
	}

	data_encode(&p, hsync, vsync, true, tmds0, tmds1, tmds2);
	tmds0 += PACKET_BODY_LENGTH / 2;
	tmds1 += PACKET_BODY_LENGTH / 2;
	tmds2 += PACKET_BODY_LENGTH / 2;

	// trailing guard band (2 pixels)
	*(tmds0++) = terc4_sync;
	*(tmds1++) = DATA_ISLAND_GUARD_BAND;
	*(tmds2++) = DATA_ISLAND_GUARD_BAND;
}