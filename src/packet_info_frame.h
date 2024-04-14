#ifndef PACKET_INFO_FRAME
#define PACKET_INFO_FRAME

#include "packets.h"

void packet_avi_info(struct packet* p);
void packet_spd_info(struct packet* p, const char* vendor, const char* product);
void packet_audio_info(struct packet* p);
void packet_audio_clk_regen(struct packet* p, uint32_t n, uint32_t cts);

#endif