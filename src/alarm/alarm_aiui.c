#define TAG "alarm_aiui"

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <cJSON.h>
#include <sys/time.h>
#include <stdbool.h>
#include "lisa_mem.h"
#include "lisa_log.h"
#include "alarm.h"
#include "alarm_aiui.h"
#include "alarm_store.h"
#include "listen_system.h"

extern char *strptime(const char *s, const char *format, struct tm *tm);

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
	AIUI_ALARM_SLOTS_ITEM_TYPE_NAME,
	AIUI_ALARM_SLOTS_ITEM_TYPE_REPEAT,
	AIUI_ALARM_SLOTS_ITEM_TYPE_ALL,
};

typedef void (*alarm_aiui_intent_handle_t)(uint64_t timestamp, const uint8_t *text);
static void alarm_aiui_create_intent_handle(uint64_t timestamp, const uint8_t *text);
static void alarm_aiui_cancel_intent_handle(uint64_t timestamp, const uint8_t *text);
static void alarm_aiui_unknown_intent_handle(uint64_t timestamp, const uint8_t *text);

static alarm_aiui_user_callback_t g_alarm_aiui_user_callback = NULL;

const uint8_t *aiui_alarm_intent_string[AIUI_ALARM_INTENT_MAX] = {
		[AIUI_ALARM_INTENT_CREATE] = "CREATE",
		[AIUI_ALARM_INTENT_CANCEL] = "CANCEL",
		[AIUI_ALARM_INTENT_VIEW] = "VIEW",
};

const alarm_aiui_intent_handle_t alarm_aiui_intent_handles[AIUI_ALARM_INTENT_MAX] = {
		[AIUI_ALARM_INTENT_UNKNOWN] = alarm_aiui_unknown_intent_handle,
		[AIUI_ALARM_INTENT_CREATE] = alarm_aiui_create_intent_handle,
		[AIUI_ALARM_INTENT_CANCEL] = alarm_aiui_cancel_intent_handle,
};

static void alarm_aiui_unknown_intent_handle(uint64_t timestamp, const uint8_t *text)
{
	LISA_LOGI(TAG,"unsupported intent");
}

static void alarm_aiui_create_intent_handle(uint64_t timestamp, const uint8_t *text)
{
	if (timestamp == 0) {
		LISA_LOGI(TAG, "Invalid alarm timestamp");
		return;
	}

	if (text && strlen((char*)text) > 0) {
		alarm_store_create_obj_by_timestamp(timestamp, text);
		ls_alarm_insert_by_timestamp(timestamp, text);
	} else {
		alarm_store_create_obj_by_timestamp(timestamp, (uint8_t*)"");
		ls_alarm_insert_by_timestamp(timestamp, (uint8_t*)"");
	}
}

static void alarm_aiui_cancel_intent_handle(uint64_t timestamp, const uint8_t *text)
{
	int err = ls_alarm_delete_by_timestamp(timestamp);
	if (err) {
		LISA_LOGI(TAG, "delete alarm err:%d", err);
	}

	err = alarm_store_delete_obj_by_timestamp(timestamp);
	if (err) {
		LISA_LOGI(TAG, "delete alarm err:%d", err);
	}
}

static void alarm_aiui_intent_handle_without_precise_time(int intent, int type)
{
    LISA_LOGI(TAG, "Handle alarm intent without precise time: intent=%d, type=%d", intent, type); 

	// 获取当前时间
	struct timeval current_timestamp = {0};
	struct tm current_tm = {0};
	char current_datetime[32];
	time_t current_time = 0;

	ls_sys_get_time(&current_timestamp);

	ls_sys_get_tmtime((const long int *)&current_timestamp.tv_sec, &current_tm);
	strftime(current_datetime, sizeof(current_datetime), "%Y-%m-%dT%H:%M:%S", &current_tm);
    LISA_LOGI(TAG, "current time: %s, timestamp: %lld", current_datetime, (int64_t)current_timestamp.tv_sec);
	
	current_time =  (int64_t)current_timestamp.tv_sec;

    if (intent == AIUI_ALARM_INTENT_CREATE) {
		// 没有精确闹钟时间，设置默认闹钟30分钟
        uint64_t default_time = current_time + 30 * 60;
        ls_alarm_insert_by_timestamp(default_time, (uint8_t*)"");
        LISA_LOGI(TAG,"Create a default alarm after 30 minutes: %lld", default_time);
    }
	else if (intent == AIUI_ALARM_INTENT_CANCEL) {
		// 没有精确闹钟时间，删除最近的闹钟或全部闹钟
		if (type == AIUI_ALARM_SLOTS_ITEM_TYPE_ALL) {
			ls_alarm_clear_all();
			LISA_LOGI(TAG,"Cleared all alarms");
		} else {
			struct ls_alarm *next_alarm = ls_alarm_get();
			if (next_alarm) {
				uint64_t alarm_time = next_alarm->timestamp;
				ls_alarm_delete_by_timestamp(alarm_time);
				LISA_LOGI(TAG,"Deleted next alarm at timestamp: %lld", alarm_time);
			} else {
				LISA_LOGI(TAG,"No alarms to delete");
			}
		}
	}
}

static void alarm_aiui_intent_handle_dispatch(
        int intent, int type, const char *datetime_string, const uint8_t *text)
{
	LISA_LOGI(TAG,"ALARM_INTENT: intent=%d, type=%d, datetime=%s, text=%s", 
			  intent,
			  type,
			  datetime_string ? datetime_string : "NULL",
			  text ? (const char *)text : "NULL");
    
    if (intent >= AIUI_ALARM_INTENT_MAX) {
        return;
    }
    
    if (intent == AIUI_ALARM_INTENT_UNKNOWN) {
        alarm_aiui_unknown_intent_handle(0, NULL);
        return;
    }
    

	// 解析datetime字符串，转换为时间戳
	uint64_t alarm_timestamp = 0;
	bool precise_time = false; 
	if (datetime_string && datetime_string[0] != '\0') {
		precise_time = true; 
		char *p;
		p = strstr(datetime_string, "T");
		precise_time &= (p != NULL);
		p = strstr(datetime_string, "/");
		precise_time &= (p == NULL);


		if (precise_time) {
			// 获取当前时间
			struct timeval current_timestamp = {0};
			struct tm current_tm = {0};
			char current_datetime[32];

			if (ls_sys_get_time(&current_timestamp) != 0 || current_timestamp.tv_sec == 0) {
				LISA_LOGE(TAG, "ls_sys_get_time failed or returned 0, abort compare");
				return;
			}

			ls_sys_get_tmtime((const long int *)&current_timestamp.tv_sec, &current_tm);
			strftime(current_datetime, sizeof(current_datetime), "%Y-%m-%dT%H:%M:%S", &current_tm);
            
            LISA_LOGI(TAG,"compare time: alarm_time=%s, current_time=%s", 
                    datetime_string, current_datetime);
            
			// 闹钟时间小于等于当前时间，调整闹钟时间到明天		
            if (strcmp(datetime_string, current_datetime) <= 0) {
                LISA_LOGI(TAG,"The alarm clock time has expired, adjust it to the same time tomorrow");
                
				struct tm time_info = {0};
				strptime(datetime_string, "%Y-%m-%dT%H:%M:%S", &time_info);
				alarm_timestamp = mktime(&time_info);
				/* Adjust for local CST (UTC+8) if libc timezone is UTC */
				alarm_timestamp -= 8 * 60 * 60;
				alarm_timestamp += 24 * 60 * 60;
                
            } else {
				struct tm time_info = {0};
				strptime(datetime_string, "%Y-%m-%dT%H:%M:%S", &time_info);
				alarm_timestamp = mktime(&time_info);
				/* Adjust for local CST (UTC+8) if libc timezone is UTC */
				alarm_timestamp -= 8 * 60 * 60;
            }
            LISA_LOGI(TAG,"Final alarm timestamp: %lld", alarm_timestamp);
        }
    }
    
	// 根据是否有精确时间处理闹钟
    if (!precise_time) {
    	LISA_LOGI(TAG, "Handle alarm intent without precise time: intent=%d, type=%d", intent, type); 

        alarm_aiui_intent_handle_without_precise_time(intent, type);
        return;
    }
    
    alarm_aiui_intent_handle_t handle = alarm_aiui_intent_handles[intent];
    if (handle) {
        handle(alarm_timestamp, text);
    }
}


static int alarm_aiui_slots_item_parse(cJSON *slots_item, char **out)
{
	*out = NULL;

	if (slots_item == NULL) {
		LISA_LOGE(TAG, "slots_item is NULL");
		return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
	}

	cJSON *slots_item_name = cJSON_GetObjectItem(slots_item, "name");
	if (slots_item_name == NULL || slots_item_name->valuestring == NULL) {
		LISA_LOGE(TAG, "slots_item name is invalid");
		return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
	}

	LISA_LOGI(TAG,"slots item name:%s", slots_item_name->valuestring);

	if (strcmp(slots_item_name->valuestring, "datetime") == 0) {
		cJSON *normValue = cJSON_GetObjectItem(slots_item, "normValue");
		if (normValue == NULL || normValue->valuestring == NULL) {
			LISA_LOGE(TAG, "normValue is invalid for datetime slot");
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
		}

		cJSON *normValueJson = cJSON_Parse(normValue->valuestring);
		if (normValueJson == NULL) {
			LISA_LOGE(TAG, "Failed to parse normValue JSON: %s", normValue->valuestring);
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
		}

		cJSON *suggestDatetime = cJSON_GetObjectItem(normValueJson, "suggestDatetime");
		if (suggestDatetime == NULL || suggestDatetime->valuestring == NULL) {
			LISA_LOGE(TAG, "suggestDatetime is invalid in normValue");
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
		if (value == NULL || value->valuestring == NULL) {
			LISA_LOGE(TAG, "value is invalid for content slot");
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
		}

		int tmp_len = strlen(value->valuestring) + 1;
		char *tmp = lisa_mem_alloc(tmp_len);
		if (tmp) {
			memset(tmp, 0, tmp_len);
			strcat(tmp, value->valuestring);
			*out = tmp;
		} else {
			LISA_LOGE(TAG, "Failed to allocate memory for content string");
		}

		return AIUI_ALARM_SLOTS_ITEM_TYPE_CONTENT;
	}  else if (strcmp(slots_item_name->valuestring, "name") == 0) {
		cJSON *value = cJSON_GetObjectItem(slots_item, "value");
		if (value == NULL || value->valuestring == NULL) {
			LISA_LOGE(TAG, "value is invalid for name slot");
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ERROR;
		}

		int tmp_len = strlen(value->valuestring) + 1;
		char *tmp = lisa_mem_alloc(tmp_len);
		if (tmp) {
			memset(tmp, 0, tmp_len);
			strcat(tmp, value->valuestring);
			*out = tmp;
		} else {
			LISA_LOGE(TAG, "Failed to allocate memory for name string");
		}

		return AIUI_ALARM_SLOTS_ITEM_TYPE_NAME;
	}else if (strcmp(slots_item_name->valuestring, "property") == 0) {
		cJSON *value = cJSON_GetObjectItem(slots_item, "value");
		if (strcmp(value->valuestring, "all") == 0) {
			return AIUI_ALARM_SLOTS_ITEM_TYPE_ALL;
		}
	} else if (strcmp(slots_item_name->valuestring, "repeat") == 0) {
		return AIUI_ALARM_SLOTS_ITEM_TYPE_REPEAT;
	} 

	return AIUI_ALARM_SLOTS_ITEM_TYPE_UNKNOWN;
}

static int alarm_aiui_intent_convert(const uint8_t *intent_string)
{
	if (intent_string == NULL) {
		return AIUI_ALARM_INTENT_UNKNOWN;
	}
	
	for (int i = 0; i < AIUI_ALARM_INTENT_MAX; i++) {
		const char *s = (const char *)aiui_alarm_intent_string[i];
		if (s != NULL && strcmp(s, (const char *)intent_string) == 0) {
			return i;
		}
	}

	return AIUI_ALARM_INTENT_UNKNOWN;
}

/* 兼容 1.2.3 与 2.0.0 云端链路 */
int alarm_aiui_process_intent_and_slots(const char *intent_str, cJSON *slots_array)
{
	if (!intent_str || !slots_array) {
		return -1;
	}

	int alarm_intent = alarm_aiui_intent_convert((const uint8_t *)intent_str);

	int size = cJSON_GetArraySize(slots_array);
	if (size <= 0) {
		return -1;
	}
	char *datetime_string = NULL;
	char *content_string = NULL;
	char *name_string = NULL;

	int alarm_type = AIUI_ALARM_SLOTS_ITEM_TYPE_UNKNOWN;

	for (int i = 0; i < size; i++) {
		LISA_LOGI(TAG, "item size:%d, curr:%d", size, i);
		cJSON *slots_item = cJSON_GetArrayItem(slots_array, i);
		char *temp = NULL;
		alarm_type = alarm_aiui_slots_item_parse(slots_item, &temp);

		if (alarm_type == AIUI_ALARM_SLOTS_ITEM_TYPE_DATETIME) {
			datetime_string = temp;
		} else if (alarm_type == AIUI_ALARM_SLOTS_ITEM_TYPE_CONTENT) {
			content_string = temp;
		} else if (alarm_type == AIUI_ALARM_SLOTS_ITEM_TYPE_NAME) {
			name_string = temp;
		} else if (alarm_type == AIUI_ALARM_SLOTS_ITEM_TYPE_REPEAT) {
			/* not support */
			alarm_type = AIUI_ALARM_SLOTS_ITEM_TYPE_UNKNOWN;
			LISA_LOGI(TAG, "slots break iteration, curr index:%d", i);
			break;
		}
	}

	alarm_aiui_intent_handle_dispatch(alarm_intent, alarm_type, datetime_string, content_string);
	lisa_mem_free(datetime_string);
	lisa_mem_free(content_string);
	lisa_mem_free(name_string);

	return 0;
}

/* 1.2.3 链路接口 */
int alarm_aiui_intent_process(cJSON *semantic_root)
{
	LISA_LOGI(TAG,"%s---", __func__);
	cJSON *semantic = cJSON_GetObjectItem(semantic_root, "semantic");
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
	if (intent == NULL || !cJSON_IsString(intent)) {
		return -1;
	}

	cJSON *slots = cJSON_GetObjectItem(sema_item, "slots");
	if (slots == NULL || !cJSON_IsArray(slots)) {
		return -1;
	}

	int ret = alarm_aiui_process_intent_and_slots(intent->valuestring, slots);

	return ret;
}

/* ==================== 初始化 ==================== */

void alarm_aiui_register_callback(ls_alarm_user_callback_t callback)
{
    LISA_LOGI(TAG,"Registering alarm callback function: %p", callback);
    g_alarm_aiui_user_callback = callback;
}

int alarm_aiui_init(alarm_aiui_user_callback_t cb)
{
    if (!cb) {
        LISA_LOGE(TAG, "Invalid callback pointer");
        return -1;
    }

    alarm_aiui_register_callback(cb);
    ls_alarm_init((ls_alarm_user_callback_t)cb);

    struct timeval current_time = {0};
	ls_sys_get_time(&current_time);
    LISA_LOGI(TAG,"Alarm system initialization complete, current timestamp: %lld", (int64_t)current_time.tv_sec);

    return 0;
}
