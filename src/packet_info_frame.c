#include <string.h>

#include "packet_info_frame.h"

static void apply_checksum(struct packet* p) {
	uint8_t sum = 0;

	for(size_t k = 0; k < sizeof(p->header) - 1; k++)
		sum += p->header[k];

	for(size_t k = 0; k < sizeof(p->data); k++) {
		if(k % 8 != 7)
			sum += p->data[k];
	}

	p->data[0] = -sum;
}

void packet_avi_info(struct packet* p) {
	memset(p->header, 0, sizeof(p->header));
	memset(p->data, 0, sizeof(p->data));

	p->header[0] = 0x82;
	p->header[1] = 0x02;
	p->header[2] = 13;

	p->data[1] = 0b00100010;
	p->data[2] = 0b01101000;
	p->data[4] = 3;

	apply_checksum(p);
}

// vendor max 8
// product max 16
void packet_spd_info(struct packet* p, const char* vendor,
					 const char* product) {
	memset(p->header, 0, sizeof(p->header));
	memset(p->data, 0, sizeof(p->data));

	p->header[0] = 0x83;
	p->header[1] = 0x01;
	p->header[2] = 25;

	strncpy((char*)p->data + 1, vendor, 8);
	strncpy((char*)p->data + 10, product, 16);
	p->data[28] = 0x08; // game

	apply_checksum(p);
}