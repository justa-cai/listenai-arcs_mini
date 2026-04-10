#ifndef __ALARM_H__
#define __ALARM_H__

#include "stdint.h"

#define LS_ALARM_MAX_COUNT (50) 

struct ls_alarm;
typedef void (*ls_alarm_callbacks_t)(struct ls_alarm *alarm, void *data);
typedef void (*ls_alarm_user_callback_t)(uint64_t timestamp, const uint8_t *text);

#ifndef CONFIG_ALARM_TEXT_MAX_LEN
#define CONFIG_ALARM_TEXT_MAX_LEN 384
#endif

/* 100 个中文 UTF-8 字符约 300 字节，预留额外空间避免提醒文案被截断。 */
#define LS_ALARM_TEXT_MAX_LEN CONFIG_ALARM_TEXT_MAX_LEN
struct ls_alarm {
	struct ls_alarm *prev;
	struct ls_alarm *next;
	uint64_t timestamp;   // alarm_id，用于排序和定时
	uint64_t cloud_id;    // 云端ID，用于 NVS 操作
	ls_alarm_callbacks_t cb;
	uint8_t text[LS_ALARM_TEXT_MAX_LEN];
};

void ls_alarm_init(ls_alarm_user_callback_t cb);
int ls_alarm_insert_by_timestamp(uint64_t timestamp, uint64_t cloud_id, const uint8_t *text);
int ls_alarm_delete_by_timestamp(uint64_t timestamp);
struct ls_alarm *ls_alarm_get(void);
int ls_alarm_count_get(void);

#endif
