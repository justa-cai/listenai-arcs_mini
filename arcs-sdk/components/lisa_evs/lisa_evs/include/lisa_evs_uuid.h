#ifndef __LISA_EVS_UUID_H__
#define __LISA_EVS_UUID_H__

#include <stdint.h>

#define UUID_STRING_LENGTH (36)
#define UUID_SIZE (UUID_STRING_LENGTH + 1)

typedef struct uuid_s {
	uint8_t uuid[16];
} uuid_t;

typedef struct uuid_string_s {
	/* XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX */
	uint8_t uuid[UUID_SIZE];
} uuid_string_t;

int lisa_evs_uuid_generate_random(uuid_t *uuid);
uuid_string_t lisa_evs_uuid_to_string(uuid_t const *uuid);
void lisa_evs_uuid_generate_string(uint8_t *out);

#endif // __LISA_EVS_UUID_H__
