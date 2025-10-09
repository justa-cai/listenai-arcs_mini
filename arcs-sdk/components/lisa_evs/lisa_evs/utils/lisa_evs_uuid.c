#include <string.h>
#include <stdio.h>
#include "lisa_evs_uuid.h"
#include "lisa_time.h"
#include "lisa_log.h"

#define TAG "lisa_uuid"

typedef struct uuid_decomposed_s {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq_hi_and_reserved;
	uint8_t clock_seq_low;
	uint8_t node[6];
} uuid_decomposed_t;

int lisa_evs_uuid_generate_random(uuid_t *uuid)
{
	uint16_t *p = (uint16_t *)uuid;
	while ((char *)p < (char *)(uuid + 1)) {
		*p++ = lisa_rand32();
	}

	uuid->uuid[6] &= 0x0F;
	uuid->uuid[6] |= 0x40;
	uuid->uuid[8] &= 0x3F;
	uuid->uuid[8] |= 0x80;
	return 0;
}

uuid_string_t lisa_evs_uuid_to_string(uuid_t const *uuid)
{
	uuid_string_t rslt;
	uuid_decomposed_t const *u = (uuid_decomposed_t const *)uuid;

	snprintf(rslt.uuid, sizeof(rslt.uuid),
			"%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x", u->time_low, u->time_mid,
			u->time_hi_and_version, u->clock_seq_hi_and_reserved, u->clock_seq_low, u->node[0],
			u->node[1], u->node[2], u->node[3], u->node[4], u->node[5]);
	return rslt;
}

void lisa_evs_uuid_generate_string(uint8_t *out)
{
	uuid_t uuid = {{0}};
	lisa_evs_uuid_generate_random(&uuid);
	uuid_string_t uuidStr = lisa_evs_uuid_to_string(&uuid);
	strcpy(out, uuidStr.uuid);
	LISA_LOGD(TAG, "out uuid %s, len: %d", out, strlen(out));
}
