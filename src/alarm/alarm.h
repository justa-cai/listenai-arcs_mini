#ifndef __ALARM_H__
#define __ALARM_H__

#include "stdint.h"

#define LS_ALARM_MAX_COUNT (20)

struct ls_alarm;
typedef void (*ls_alarm_callbacks_t)(struct ls_alarm *alarm, void *data);
typedef void (*ls_alarm_user_callback_t)(uint64_t timestamp, const uint8_t *text);

#define LS_ALARM_TEXT_MAX_LEN 128
struct ls_alarm {
	struct ls_alarm *prev;
	struct ls_alarm *next;
	uint64_t timestamp;
	ls_alarm_callbacks_t cb;
	uint8_t text[LS_ALARM_TEXT_MAX_LEN];
};

struct ls_alarm_nvs {
    uint64_t timestamp;
	uint32_t text_len;
    uint8_t text[LS_ALARM_TEXT_MAX_LEN];
};

#define NVS_ALARM_KEY "user.alarm.keys"

void ls_alarm_init(ls_alarm_user_callback_t cb);
int ls_alarm_insert_by_timestamp(uint64_t timestamp, const uint8_t *text);
int ls_alarm_delete_by_timestamp(uint64_t timestamp);
struct ls_alarm *ls_alarm_get(void);

#endif
