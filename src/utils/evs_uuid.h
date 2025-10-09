#ifndef __LISTENAI_UUID_H__
#define __LISTENAI_UUID_H__

#include <stdint.h>

struct uuid {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint16_t clock_seq;
	uint8_t node[6];
};

typedef unsigned char uuid_t[16];

#define UUID_STRING_LENGTH 36
#define UUID_SIZE (UUID_STRING_LENGTH + 1)

void evs_uuid_generate_string(char *out);

#endif
