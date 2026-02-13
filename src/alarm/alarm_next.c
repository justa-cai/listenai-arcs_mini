#include "alarm_next.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "alarm_store.h"
#include "lisa_log.h"
#include "lisa_http.h"
#include "lisa_kv.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include "kv_user.h"

#define TAG "alarm_next"

static int alarm_update_trigger_from_timestamp(alarm_object_t *alarm, uint64_t timestamp)
{
	if (!alarm || timestamp == 0) {
		return -1;
	}

	time_t ts = (time_t)timestamp;
	struct tm tmv;
	memset(&tmv, 0, sizeof(tmv));
	if (!localtime_r(&ts, &tmv)) {
		return -1;
	}

	alarm->trigger.year = (uint16_t)(tmv.tm_year + 1900);
	alarm->trigger.month = (uint8_t)(tmv.tm_mon + 1);
	alarm->trigger.day = (uint8_t)tmv.tm_mday;
	alarm->trigger.hour = (uint8_t)tmv.tm_hour;
	alarm->trigger.minute = (uint8_t)tmv.tm_min;
	alarm->trigger.second = (uint8_t)tmv.tm_sec;

	return 0;
}

static char *s_listenai_date_headers = NULL;

static void *listenai_date_headers_cb(void)
{
	return (void *)s_listenai_date_headers;
}

static void listenai_date_json_on_data(lisa_http_data_t *data)
{
	cJSON **json = (cJSON **)data->user;
	if (!json || !data || !data->buf) {
		return;
	}

	if (*json) {
		cJSON_Delete(*json);
		*json = NULL;
	}
	*json = cJSON_ParseWithLength((const char *)data->buf, data->len);
}

static cJSON *listenai_date_transition_json(const char *type, const char *date_name, int *out_err)
{
	if (out_err) *out_err = -1;
	if (!type || !date_name) return NULL;

	// 请求地址
	char url[128] = {0};
	const char *host_suffix = "";
	int device_mode = 0;
	if (lisa_kv_get_int(KV_KEY_STAGING, &device_mode) != 0) {
		device_mode = 0;
	}
	if (device_mode == 1) {
		host_suffix = "staging-";
	} else if (device_mode == 2) {
		host_suffix = "integration-";
	}
	snprintf(url, sizeof(url), "https://%sapi.listenai.com/v1/date/transition", host_suffix);

	// 请求头
	const char *header_fmt = "Content-Type: application/json\r\nAuthorization: Bearer %s";

	char *token = NULL;
	if (lisa_kv_get_string(KV_KEY_TOKEN, &token) != 0 || token == NULL) {
		if (out_err) *out_err = -2;
		return NULL;
	}
	size_t header_len = strlen(header_fmt) + strlen(token) + 1;
	if (s_listenai_date_headers) {
		lisa_mem_free(s_listenai_date_headers);
		s_listenai_date_headers = NULL;
	}

	s_listenai_date_headers = lisa_mem_calloc(1, header_len);
	if (!s_listenai_date_headers) {
		lisa_kv_free(token);
		if (out_err) *out_err = -3;
		return NULL;
	}
	snprintf(s_listenai_date_headers, header_len, header_fmt, token);
	lisa_kv_free(token);

	// 请求体
	char body[128];
	int body_len = snprintf(body, sizeof(body), "{\"date_name\":\"%s\",\"type\":\"%s\"}", date_name, type);
	if (body_len < 0 || body_len >= (int)sizeof(body)) {
		lisa_mem_free(s_listenai_date_headers);
		s_listenai_date_headers = NULL;
		if (out_err) *out_err = -4;
		return NULL;
	}

	cJSON *json = NULL;
	lisa_http_request_t req = {
		.method = LISA_HTTP_POST,
		.url = (uint8_t *)url,
		.timeout = 10,
		.body = body,
		.body_len = body_len,
		.headers = (uint8_t *)listenai_date_headers_cb,
		.on_data = listenai_date_json_on_data,
		.user = &json,
	};

	lisa_http_t *http = lisa_http_init(&req);
	if (!http) {
		lisa_mem_free(s_listenai_date_headers);
		s_listenai_date_headers = NULL;
		if (out_err) *out_err = -9;
		return NULL;
	}

	lisa_http_err_e err = lisa_http_perform(http);
	lisa_http_cleanup(http);

	if (s_listenai_date_headers) {
		lisa_mem_free(s_listenai_date_headers);
		s_listenai_date_headers = NULL;
	}

	if (err != LISA_HTTP_OK) {
		if (json) cJSON_Delete(json);
		if (out_err) *out_err = -10;
		return NULL;
	}

	if (!json && out_err) {
		*out_err = -5;
	}
	return json;
}

int listenai_date_transition(const char *type, const char *date_name, char *out_date, size_t out_len)
{
	if (!type || !date_name) return -1;
	if (out_date == NULL) {
		if (out_len != 0) return -1;
	} else if (out_len < 11) {
		return -1;
	}

	int err = 0;
	cJSON *json = listenai_date_transition_json(type, date_name, &err);
	if (!json) {
		return err;
	}

	char *json_str = cJSON_PrintUnformatted(json);
	if (json_str) {
		LISA_LOGI(TAG, "date transition response: type=%s name=%s resp=%s", type, date_name, json_str);
		cJSON_free(json_str);
	}

	cJSON *code = cJSON_GetObjectItem(json, "code");
	if (cJSON_IsNumber(code) && code->valueint != 0) {
		cJSON_Delete(json);
		return -6;
	}

	cJSON *date = cJSON_GetObjectItem(json, "date");
	if (!cJSON_IsString(date) || date->valuestring == NULL) {
		cJSON_Delete(json);
		return -7;
	}

	if (out_date && out_len > 0) {
		strncpy(out_date, date->valuestring, out_len - 1);
		out_date[out_len - 1] = '\0';
	}

	cJSON_Delete(json);
	return 0;
}

int listenai_date_is_workday(const char *date_name, bool *out_is_work_day)
{
	if (!date_name || !out_is_work_day) return -1;

	int err = 0;
	cJSON *json = listenai_date_transition_json("workday", date_name, &err);
	if (!json) {
		return err;
	}

	char *json_str = cJSON_PrintUnformatted(json);
	if (json_str) {
		LISA_LOGI(TAG, "date workday response: name=%s resp=%s", date_name, json_str);
		cJSON_free(json_str);
	}

	cJSON *code = cJSON_GetObjectItem(json, "code");
	if (cJSON_IsNumber(code) && code->valueint != 0) {
		cJSON_Delete(json);
		return -6;
	}

	cJSON *is_work_day = cJSON_GetObjectItem(json, "is_work_day");
	if (cJSON_IsBool(is_work_day)) {
		*out_is_work_day = cJSON_IsTrue(is_work_day);
	} else if (cJSON_IsNumber(is_work_day)) {
		*out_is_work_day = is_work_day->valueint != 0;
	} else {
		cJSON_Delete(json);
		return -7;
	}

	cJSON_Delete(json);
	return 0;
}

static int days_in_month(int year, int month)
{
	static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month < 1 || month > 12) {
		return 30;
	}
	if (month == 2) {
		bool leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
		return leap ? 29 : 28;
	}
	return days[month - 1];
}

static uint64_t alarm_calc_next_trigger_lunar(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	if (!alarm) {
		return 0;
	}

	switch (alarm->trigger.type) {
	case ALARM_TRIG_DAILY:
	{
		uint64_t next = base;
		while (next <= (uint64_t)now_ts) {
			next += 24 * 60 * 60;
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_WEEKLY:
	{
		uint64_t next = base;
		while (next <= (uint64_t)now_ts) {
			next += 7 * 24 * 60 * 60;
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_MONTHLY:
	{
		if (alarm->trigger.lunar_month == 0 || alarm->trigger.lunar_day == 0) {
			LISA_LOGE(TAG, "lunar monthly missing lunar month/day");
			return 0;
		}
		uint8_t next_lunar_month = alarm->trigger.lunar_month;
		alarm_object_t temp = *alarm;
		for (int i = 0; i < 24; i++) {
			next_lunar_month += 1;
			if (next_lunar_month > 12) {
				next_lunar_month = 1;
			}
			char lunar_date[16] = {0};
			snprintf(lunar_date, sizeof(lunar_date), "%02u-%02u",
				 next_lunar_month, alarm->trigger.lunar_day);
			char gregorian_date[16] = {0};
			if (listenai_date_transition("lunar", lunar_date, gregorian_date, sizeof(gregorian_date)) != 0) {
				LISA_LOGE(TAG, "lunar transition failed, lunar=%s", lunar_date);
				return 0;
			}
			unsigned int year = 0, month = 0, day = 0;
			if (sscanf(gregorian_date, "%u-%u-%u", &year, &month, &day) != 3) {
				LISA_LOGE(TAG, "lunar transition parse failed, date=%s", gregorian_date);
				return 0;
			}
			temp.trigger.year = (uint16_t)year;
			temp.trigger.month = (uint8_t)month;
			temp.trigger.day = (uint8_t)day;
			temp.trigger.lunar_month = next_lunar_month;
			temp.trigger.lunar_day = alarm->trigger.lunar_day;
			uint64_t next = alarm_obj_to_timestamp(&temp);
			if (next > (uint64_t)now_ts) {
				*alarm = temp;
				alarm_update_trigger_from_timestamp(alarm, next);
				return next;
			}
		}
		LISA_LOGW(TAG, "lunar monthly next not in future, now=%lld", (long long)now_ts);
		return 0;
	}
	case ALARM_TRIG_YEARLY:
	{
		if (alarm->trigger.lunar_month == 0 || alarm->trigger.lunar_day == 0) {
			LISA_LOGE(TAG, "lunar yearly missing lunar month/day");
			return 0;
		}
		char lunar_date[16] = {0};
		snprintf(lunar_date, sizeof(lunar_date), "%02u-%02u",
			 alarm->trigger.lunar_month, alarm->trigger.lunar_day);
		char gregorian_date[16] = {0};
		if (listenai_date_transition("lunar", lunar_date, gregorian_date, sizeof(gregorian_date)) != 0) {
			LISA_LOGE(TAG, "lunar transition failed, lunar=%s", lunar_date);
			return 0;
		}
		unsigned int year = 0, month = 0, day = 0;
		if (sscanf(gregorian_date, "%u-%u-%u", &year, &month, &day) != 3) {
			LISA_LOGE(TAG, "lunar transition parse failed, date=%s", gregorian_date);
			return 0;
		}
		alarm->trigger.year = (uint16_t)year;
		alarm->trigger.month = (uint8_t)month;
		alarm->trigger.day = (uint8_t)day;
		uint64_t next = alarm_obj_to_timestamp(alarm);
		if (next <= (uint64_t)now_ts) {
			LISA_LOGW(TAG, "lunar yearly next not in future, date=%s now=%lld",
				  gregorian_date, (long long)now_ts);
			return 0;
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	default:
		LISA_LOGW(TAG, "lunar trigger type not supported: %d", alarm->trigger.type);
		return 0;
	}
}

uint64_t alarm_calc_next_trigger(alarm_object_t *alarm, time_t now_ts)
{
	if (!alarm) {
		return 0;
	}

	uint64_t base = alarm_obj_to_timestamp(alarm);
	if (base == 0) {
		return 0;
	}

	if (alarm->calendar == ALARM_CAL_LUNAR) {
		return alarm_calc_next_trigger_lunar(alarm, now_ts, base);
	}

	switch (alarm->trigger.type) {
	case ALARM_TRIG_ONCE:
		return 0;
	case ALARM_TRIG_DAILY:
	{
		uint64_t next = base;
		while (next <= (uint64_t)now_ts) {
			next += 24 * 60 * 60;
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_WEEKLY:
	{
		uint64_t next = base;
		while (next <= (uint64_t)now_ts) {
			next += 7 * 24 * 60 * 60;
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_WORKDAY:
	{
		uint64_t next = base;
		while (1) {
			next += 24 * 60 * 60;
			alarm->trigger.day += 1;
			char date_str[16] = {0};
			snprintf(date_str, sizeof(date_str), "%02d-%02d", alarm->trigger.month, alarm->trigger.day);
			bool is_work_day = true;
			if (listenai_date_is_workday(date_str, &is_work_day) != 0) {
				LISA_LOGE(TAG, "workday transition failed, date=%s", date_str);
				return 0;
			}
			if (is_work_day) {
				break;
			}
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_WEEKEND:
	{
		uint64_t next = base;
		struct tm tmv;
		const uint64_t day_sec = 24 * 60 * 60;
		while (1) {
			time_t ts = (time_t)next;
			if (!localtime_r(&ts, &tmv)) {
				next += day_sec;
				continue;
			}
			int wd = tmv.tm_wday; // 0=Sun .. 6=Sat
			bool is_weekend = (wd == 0 || wd == 6);
			if (next > (uint64_t)now_ts && is_weekend) {
				break;
			}
			int step_days = 1;
			if (wd == 6) {
				step_days = 1; // Sat -> Sun
			} else if (wd == 0) {
				step_days = 6; // Sun -> next Sat
			} else {
				step_days = 6 - wd; // Mon..Fri -> Sat
			}
			next += (uint64_t)step_days * day_sec;
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_MONTHLY:
	{
		uint8_t desired_day = alarm->trigger.day;
		if (alarm->trigger.day_of_month > 0) {
			desired_day = alarm->trigger.day_of_month;
		}
		alarm_object_t temp = *alarm;
		uint64_t next = base;
		while (next <= (uint64_t)now_ts) {
			temp.trigger.month += 1;
			if (temp.trigger.month > 12) {
				temp.trigger.month = 1;
				temp.trigger.year += 1;
			}
			int max_day = days_in_month(temp.trigger.year, temp.trigger.month);
			if (desired_day > (uint8_t)max_day) {
				temp.trigger.day = (uint8_t)max_day;
			} else {
				temp.trigger.day = desired_day;
			}
			next = alarm_obj_to_timestamp(&temp);
		}
		*alarm = temp;
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_YEARLY:
	{
		uint8_t desired_day = alarm->trigger.day;
		if (alarm->trigger.day_of_month > 0) {
			desired_day = alarm->trigger.day_of_month;
		}
		if (alarm->calendar == ALARM_CAL_LUNAR) {
			char lunar_date[16] = {0};
			snprintf(lunar_date, sizeof(lunar_date), "%02u-%02u",
				 alarm->trigger.lunar_month, alarm->trigger.lunar_day);
			char gregorian_date[16] = {0};
			if (listenai_date_transition("lunar", lunar_date, gregorian_date, sizeof(gregorian_date)) != 0) {
				LISA_LOGE(TAG, "lunar transition failed, lunar=%s", lunar_date);
				return 0;
			}
			unsigned int year = 0, month = 0, day = 0;
			if (sscanf(gregorian_date, "%u-%u-%u", &year, &month, &day) != 3) {
				LISA_LOGE(TAG, "lunar transition parse failed, date=%s", gregorian_date);
				return 0;
			}
			alarm->trigger.year = (uint16_t)year;
			alarm->trigger.month = (uint8_t)month;
			alarm->trigger.day = (uint8_t)day;
			uint64_t next = alarm_obj_to_timestamp(alarm);
			if (next <= (uint64_t)now_ts) {
				LISA_LOGW(TAG, "lunar next not in future, date=%s now=%lld",
					  gregorian_date, (long long)now_ts);
				return 0;
			}
			alarm_update_trigger_from_timestamp(alarm, next);
			return next;
		}

		alarm_object_t temp = *alarm;
		uint64_t next = base;
		while (next <= (uint64_t)now_ts) {
			temp.trigger.year += 1;
			int max_day = days_in_month(temp.trigger.year, temp.trigger.month);
			if (desired_day > (uint8_t)max_day) {
				temp.trigger.day = (uint8_t)max_day;
			} else {
				temp.trigger.day = desired_day;
			}
			next = alarm_obj_to_timestamp(&temp);
		}
		*alarm = temp;
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	case ALARM_TRIG_HOLIDAY:
	{
		char gregorian_date[16] = {0};
		if (listenai_date_transition("holiday", alarm->trigger.holiday_name,
					    gregorian_date, sizeof(gregorian_date)) != 0) {
			LISA_LOGE(TAG, "holiday transition failed, name=%s", alarm->trigger.holiday_name);
			return 0;
		}
		unsigned int year = 0, month = 0, day = 0;
		if (sscanf(gregorian_date, "%u-%u-%u", &year, &month, &day) != 3) {
			LISA_LOGE(TAG, "holiday transition parse failed, date=%s", gregorian_date);
			return 0;
		}
		alarm->trigger.year = (uint16_t)year;
		alarm->trigger.month = (uint8_t)month;
		alarm->trigger.day = (uint8_t)day;
		uint64_t next = alarm_obj_to_timestamp(alarm);
		if (next <= (uint64_t)now_ts) {
			LISA_LOGW(TAG, "holiday next not in future, date=%s now=%lld",
				  gregorian_date, (long long)now_ts);
			return 0;
		}
		alarm_update_trigger_from_timestamp(alarm, next);
		return next;
	}
	default:
		return 0;
	}
}
