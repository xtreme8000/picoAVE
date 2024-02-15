#ifndef PACKET_INFO_FRAME
#define PACKET_INFO_FRAME

#include "packets.h"

void packet_avi_info(struct packet* p);
void packet_spd_info(struct packet* p, const char* vendor, const char* product);

#endif