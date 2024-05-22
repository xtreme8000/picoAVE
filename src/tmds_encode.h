#ifndef TMDS_ENCODE_H
#define TMDS_ENCODE_H

#include <stdbool.h>
#include <stdint.h>

#define DATA_ISLAND_PREAMBLE_LENGTH 8
#define DATA_ISLAND_GUARD_BAND 0x4CD33

#define VIDEO_DATA_PREAMBLE_LENGTH 8
#define VIDEO_DATA_GUARD_BAND_0 0xb32cc
#define VIDEO_DATA_GUARD_BAND_1 0x4cd33
#define VIDEO_DATA_GUARD_BAND_2 0xb32cc

extern uint16_t tmds_terc4_symbols[16];
extern uint32_t tmds_sync_symbols[4];
extern uint32_t tmds_symbols_10h[9];
extern uint32_t tmds_symbols_80h[9];

void tmds_encode_init(void);
void tmds_encode_setup(void);
uint32_t tmds_encode_y1y2(const uint32_t* pixbuf, size_t length,
						  uint32_t* tmds);
uint32_t tmds_encode_cbcr(const uint32_t* pixbuf, size_t length,
						  uint32_t* tmds);
uint32_t tmds_sync_lookup(bool vsync, bool hsync);
void tmds_encode_sync(size_t length, bool vsync, uint32_t* tmds0,
					  uint32_t* tmds1, uint32_t* tmds2);
void tmds_encode_sync_video(uint32_t* tmds0, uint32_t* tmds1, uint32_t* tmds2);

#endif