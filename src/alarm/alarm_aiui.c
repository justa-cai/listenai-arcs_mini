#include "string.h"
#include "stdint.h"
#include "cJSON.h"

#include <time.h>

#include "alarm.h"
#include "alarm_aiui.h"
#include <stdbool.h>
#include "lisa_mem.h"
#include "lisa_log.h"
#include "assistant_controller.h"

#include "sys/time.h"
#define TAG "alarm_aiui"
enum {
	AIUI_ALARM_INTENT_UNKNOWN = 0,
	AIUI_ALARM_INTENT_CREATE,
	AIUI_ALARM_INTENT_CANCEL,
	AIUI_ALARM_INTENT_VIEW,
	AIUI_ALARM_INTENT_MAX,
};

enum {
	AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR = -1,
	AIUI_ALARM_SLOTS_ITEM_TYPE_UNKNOWN = 0,
	AIUI_ALARM_SLOTS_ITEM_TYPE_DATETIME,
	AIUI_ALARM_SLOTS_ITEM_TYPE_CONTENT,
	AIUI_ALARM_SLOTS_ITEM_TYPE_REPEAT,
	AIUI_ALARM_SLOTS_ITEM_TYPE_ALL,
};

typedef void (*alarm_aiui_intent_handle_t)(uint64_t timestamp, const uint8_t *text);
static void alarm_aiui_create_intent_handle(uint64_t timestamp, const uint8_t *text);
static void alarm_aiui_cancel_intent_handle(uint64_t timestamp, const uint8_t *text);
static void alarm_aiui_unknown_intent_handle(uint64_t timestamp, const uint8_t *text);
extern void listen_client_tts(const char *text);

const uint8_t *aiui_alarm_intent_string[AIUI_ALARM_INTENT_MAX] = {
		[AIUI_ALARM_INTENT_CREATE] = "CREATE",
		[AIUI_ALARM_INTENT_CANCEL] = "CANCEL",
};

const alarm_aiui_intent_handle_t alarm_aiui_intent_handles[AIUI_ALARM_INTENT_MAX] = {
		[AIUI_ALARM_INTENT_UNKNOWN] = alarm_aiui_unknown_intent_handle,
		[AIUI_ALARM_INTENT_CREATE] = alarm_aiui_create_intent_handle,
		[AIUI_ALARM_INTENT_CANCEL] = alarm_aiui_cancel_intent_handle,
};

static void alarm_aiui_unknown_intent_handle(uint64_t timestamp, const uint8_t *text)
{
	LISA_LOGD(TAG, "unsupported intent");
}

static void alarm_aiui_create_intent_handle(uint64_t timestamp, const uint8_t *text)
{
	struct timeval tm;
	gettimeofday(&tm, NULL);
	int cnt = ls_alarm_count_get();
	LISA_LOGD(TAG, "alarm_aiui_create_intent_handle, curr time:%lld, timestamp:%lld,cnt:%d", tm.tv_sec,
			timestamp, cnt);

	if (timestamp < tm.tv_sec) {
		LISA_LOGD(TAG, "invalid timestamp");
		return;
	}

	ls_alarm_insert_by_timestamp(timestamp, text);
	assist_controller_trigger_event(CONTROLLER_EVENT_STATE_ALARM_ADD_ITEM, &timestamp, sizeof(timestamp));
}

static void alarm_aiui_cancel_intent_handle(uint64_t timestamp, const uint8_t *text)
{
	int err = ls_alarm_delete_by_timestamp(timestamp);
	if (err) {
		LISA_LOGD(TAG, "delete alarm err:%d", err);
	}
	else{
		assist_controller_trigger_event(CONTROLLER_EVENT_STATE_ALARM_DELETE_ITEM, &timestamp, sizeof(timestamp));
	}
}

static void alarm_aiui_intent_handle_without_precise_time(int intent, int type, uint64_t timestamp)
{
	LISA_LOGD(TAG, "alarm aiui intent:%d type:%d without precise time", intent, type);
}

static void alarm_aiui_intent_handle_dispatch(
		int intent, int type, const char *datetime_string, const uint8_t *text)
{
	uint64_t timestamp;
	char *p;
	bool precise_time = true;

	LISA_LOGD(TAG, "aiui alarm intent dispatch, intent:%d, type:%d, datetime:%s, content:%s",
			intent, type, datetime_string, text);

	if (intent >= AIUI_ALARM_INTENT_MAX) {
		return;
	}

	if (intent == AIUI_ALARM_INTENT_UNKNOWN) {
		alarm_aiui_unknown_intent_handle(0, NULL);
		return;
	}

	/* 2023-12-28T03:00:00, 2023-12-28 */
	p = strstr(datetime_string, "T");
	precise_time &= p != NULL;
	/* 2023-12-28T03:00:00/2023-12-28T05:00:00 */
	p = strstr(datetime_string, "/");
	precise_time &= p == NULL;

	struct tm tm = {0};

	extern char *strptime (const char *__restrict __s,
		       const char *__restrict __fmt, struct tm *__tp);
	strptime(datetime_string, "%Y-%m-%dT%H:%M:%S", &tm);
	timestamp = mktime(&tm);
	timestamp -= 60 * 60 * 8;

	LISA_LOGD(TAG, "precise time:%d", precise_time);

	if (!precise_time) {
		/* no precise time */
		alarm_aiui_intent_handle_without_precise_time(intent, type, timestamp);
		return;
	}

	alarm_aiui_intent_handle_t handle = alarm_aiui_intent_handles[intent];
	if (handle) {
		handle(timestamp, text);
	}
}

static int alarm_aiui_intent_convert(const uint8_t *intent_string)
{
	int intent = AIUI_ALARM_INTENT_UNKNOWN;
	int i;

	for (i = 0; i < AIUI_ALARM_INTENT_MAX; i++) {
		if (aiui_alarm_intent_string[i] != NULL
			&& strcmp(aiui_alarm_intent_string[i], intent_string) == 0) {
			return i;
		}
	}

	return AIUI_ALARM_INTENT_UNKNOWN;
}

static int alarm_aiui_slots_item_parse(cJSON *slots_item, char **out)
{
	*out = NULL;

	if (slots_item == NULL) {
		return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
	}

	cJSON *slots_item_name = cJSON_GetObjectItem(slots_item, "name");
	if (slots_item_name == NULL) {
		return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
	}

	LISA_LOGD(TAG, "slots item name:%s", slots_item_name->valuestring);

	if (strcmp(slots_item_name->valuestring, "datetime") == 0) {
		cJSON *normValue = cJSON_GetObjectItem(slots_item, "normValue");
		if (normValue == NULL) {
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
		}

		cJSON *normValueJson = cJSON_Parse(normValue->valuestring);
		if (normValueJson == NULL) {
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
		}

		cJSON *suggestDatetime = cJSON_GetObjectItem(normValueJson, "suggestDatetime");
		if (suggestDatetime == NULL) {
			cJSON_Delete(normValueJson);
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
		}

		/* cpy value */
		int tmp_len = strlen(suggestDatetime->valuestring) + 1;
		char *tmp = lisa_mem_alloc(tmp_len);
		if (tmp) {
			memset(tmp, 0, tmp_len);
			strcat(tmp, suggestDatetime->valuestring);
			*out = tmp;
		}

		cJSON_Delete(normValueJson);
		return AIUI_ALARM_SLOTS_ITEM_TYPE_DATETIME;
	} else if (strcmp(slots_item_name->valuestring, "content") == 0) {
		cJSON *value = cJSON_GetObjectItem(slots_item, "value");

		int tmp_len = strlen(value->valuestring) + 1;
		char *tmp = lisa_mem_alloc(tmp_len);
		if (tmp) {
			memset(tmp, 0, tmp_len);
			strcat(tmp, value->valuestring);
			*out = tmp;
		}

		return AIUI_ALARM_SLOTS_ITEM_TYPE_CONTENT;
	} else if (strcmp(slots_item_name->valuestring, "property") == 0) {
		cJSON *value = cJSON_GetObjectItem(slots_item, "value");
		if (strcmp(value->valuestring, "all") == 0) {
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ALL;
		}
	} else if (strcmp(slots_item_name->valuestring, "repeat") == 0) {
		return AIUI_ALARM_SLOTS_ITEM_TYPE_REPEAT;
	} else if (strstr(slots_item_name->valuestring, "repeat") != NULL) {
		return AIUI_ALARM_SLOTS_ITEM_TYPE_REPEAT;
	}

	return AIUI_ALARM_SLOTS_ITEM_TYPE_UNKNOWN;
}

int alarm_aiui_intent_process(cJSON *intent_root)
{
	int alarm_intent;

	cJSON *semantic = cJSON_GetObjectItem(intent_root, "semantic");
	if (semantic == NULL) {
		return -1;
	}

	int size = cJSON_GetArraySize(semantic);
	if (size <= 0) {
		return -1;
	}

	cJSON *sema_item = cJSON_GetArrayItem(semantic, 0);
	if (sema_item == NULL) {
		return -1;
	};

	cJSON *intent = cJSON_GetObjectItem(sema_item, "intent");
	if (intent == NULL) {
		return -1;
	}

	alarm_intent = alarm_aiui_intent_convert(intent->valuestring);
	cJSON *slots = cJSON_GetObjectItem(sema_item, "slots");
	if (slots == NULL) {
		return -1;
	}

	size = cJSON_GetArraySize(slots);
	if (size <= 0) {
		return -1;
	}
	char *datetime_string = NULL;
	char *content_string = NULL;

	int alarm_type = AIUI_ALARM_SLOTS_ITEM_TYPE_UNKNOWN;

	for (int i = 0; i < size; i++) {
		LISA_LOGD(TAG, "item size:%d, curr:%d", size, i);
		cJSON *slots_item = cJSON_GetArrayItem(slots, i);
		char *temp = NULL;
		alarm_type = alarm_aiui_slots_item_parse(slots_item, &temp);

		if (alarm_type == AIUI_ALARM_SLOTS_ITEM_TYPE_DATETIME) {
			datetime_string = temp;
		} else if (alarm_type == AIUI_ALARM_SLOTS_ITEM_TYPE_CONTENT) {
			content_string = temp;
		} else if (alarm_type == AIUI_ALARM_SLOTS_ITEM_TYPE_REPEAT) {
			/* not support */
			alarm_type = AIUI_ALARM_SLOTS_ITEM_TYPE_UNKNOWN;
			LISA_LOGD(TAG, "slots break iteration, curr index:%d", i);
			break;
		}
	}

	alarm_aiui_intent_handle_dispatch(alarm_intent, alarm_type, datetime_string, content_string);
	lisa_mem_free(datetime_string);
	lisa_mem_free(content_string);

	return 0;
}

int alarm_aiui_init(alarm_aiui_user_callback_t cb)
{
	ls_alarm_init((ls_alarm_user_callback_t)cb);

	return 0;
}
