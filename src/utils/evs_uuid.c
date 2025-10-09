#include "evs_uuid.h"
#include <string.h>
#include <stdio.h>
#include "lisa_time.h"

#define UUID_FMT_LOWER "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define UUID_FMT_UPPER "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"

static unsigned int arc4random3(void)
{	
	unsigned int res = (uint32_t)lisa_os_get_tick_ms();
	static unsigned int seed = 0xDEADB00B;	
	seed = ((seed & 0x007F00FF) << 7) ^		
		((seed & 0x0F80FF00) >> 8) ^ // be sure to stir those low bits		
		(res << 13) ^ (res >> 9);    // using the clock too!	

	return seed;
}

static void fill_random_num(uint8_t *buf, int len)
{	
	int left = len;	
	if (!buf)	
	{		
		return;	
	}		

	while (left >= sizeof(uint32_t))	
	{		
		unsigned int n = arc4random3();		
		memcpy(buf, &n, sizeof(uint32_t));		
		buf += sizeof(uint32_t) ;		
		left -= sizeof(uint32_t);	
	}		

	if (left > 0)	
	{		
		unsigned int n = arc4random3();		
		memcpy(buf, &n, left);	
	}
}

void get_random_bytes(void *buf, size_t len)
{	
	fill_random_num(buf, len);
}

static void __uuid_unpack(const uuid_t in, struct uuid *uu)
{
	const uint8_t *ptr = in;
	uint32_t tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	tmp = (tmp << 8) | *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_low = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_mid = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_hi_and_version = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->clock_seq = tmp;

	memcpy(uu->node, ptr, 6);
}

static void __uuid_pack(const struct uuid *uu, uuid_t ptr)
{
	uint32_t tmp;
	unsigned char *out = ptr;

	tmp = uu->time_low;
	out[3] = (unsigned char)tmp;
	tmp >>= 8;
	out[2] = (unsigned char)tmp;
	tmp >>= 8;
	out[1] = (unsigned char)tmp;
	tmp >>= 8;
	out[0] = (unsigned char)tmp;

	tmp = uu->time_mid;
	out[5] = (unsigned char)tmp;
	tmp >>= 8;
	out[4] = (unsigned char)tmp;

	tmp = uu->time_hi_and_version;
	out[7] = (unsigned char)tmp;
	tmp >>= 8;
	out[6] = (unsigned char)tmp;

	tmp = uu->clock_seq;
	out[9] = (unsigned char)tmp;
	tmp >>= 8;
	out[8] = (unsigned char)tmp;

	memcpy(out + 10, uu->node, 6);
}

static void __uuid_generate_random(uuid_t out)
{
	uuid_t buf;
	struct uuid uu;

	get_random_bytes(buf, sizeof(buf));
	__uuid_unpack(buf, &uu);

	uu.clock_seq = (uu.clock_seq & 0x3FFF) | 0x8000;
	uu.time_hi_and_version = (uu.time_hi_and_version & 0x0FFF) | 0x4000;
	__uuid_pack(&uu, out);
	out += sizeof(uuid_t);
}

static void __uuid_unparse(const uuid_t uu, char *out)
{
	struct uuid uuid;

	__uuid_unpack(uu, &uuid);
	sprintf(out, UUID_FMT_LOWER, uuid.time_low, uuid.time_mid, uuid.time_hi_and_version,
			uuid.clock_seq >> 8, uuid.clock_seq & 0xFF, uuid.node[0], uuid.node[1], uuid.node[2],
			uuid.node[3], uuid.node[4], uuid.node[5]);
}

void evs_uuid_generate_string(char *out)
{
	uuid_t id;
	__uuid_generate_random(id);
	__uuid_unparse(id, out);
}