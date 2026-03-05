#ifndef __ALARM_AIUI_H__
#define __ALARM_AIUI_H__

#include "stdint.h"
#include "cJSON.h"

int alarm_aiui_intent_process(cJSON *intent_root);
typedef void (*alarm_aiui_user_callback_t)(uint64_t timestamp, const uint8_t *text);
int alarm_aiui_init(alarm_aiui_user_callback_t cb);

int ls_alarm_count_get(void);

#endif
