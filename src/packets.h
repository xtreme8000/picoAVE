#ifndef PACKETS_H
#define PACKETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PACKET_HEADER_LENGTH 4
#define PACKET_BODY_LENGTH 32
#define PACKET_SUBLANE_LENGTH 8
#define PACKET_SUBLANE_COUNT (PACKET_BODY_LENGTH / PACKET_SUBLANE_LENGTH)

struct packet {
	uint8_t header[PACKET_HEADER_LENGTH];
	uint8_t data[PACKET_BODY_LENGTH];
};

void packets_init(void);
void packets_encode(struct packet* p, size_t amount, bool hsync, bool vsync,
					uint32_t* tmds0, uint32_t* tmds1, uint32_t* tmds2);
void packets_encode_audio(uint32_t samples[4], size_t frame, bool hsync,
						  bool vsync, uint32_t* tmds0, uint32_t* tmds1,
						  uint32_t* tmds2);
size_t packets_length(size_t amount);

#endif