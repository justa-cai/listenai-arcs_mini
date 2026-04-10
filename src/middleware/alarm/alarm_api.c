#define TAG "alarm_api"

#include "alarm_api.h"
#include <string.h>
#include <stdio.h>

#include "lisa_log.h"
#include "lisa_http.h"
#include "lisa_kv.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include "kv_user.h"
#include "lsc.h"
#include "alarm_nvs.h"
#include "alarm_constructor.h"
#include "alarm_next.h"
#include "voice_msg.h"
#include "app_datas.h"

/* ==================== 内部工具 ==================== */

static char *s_listenai_date_headers = NULL;
static char *s_alarm_sync_headers = NULL;

static void *listenai_date_headers_cb(void)
{
	return (void *)s_listenai_date_headers;
}

static void *alarm_sync_headers_cb(void)
{
	return (void *)s_alarm_sync_headers;
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
	if (lisa_kv_get_int(KV_KEY_DEVICE_MODE, &device_mode) != 0) {
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

	const char *token = get_lsc_jwt_token();
	if (token == NULL) {
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
		if (out_err) *out_err = -3;
		return NULL;
	}
	snprintf(s_listenai_date_headers, header_len, header_fmt, token);

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

/* ==================== 公开接口 ==================== */

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

int listenai_custom_alarm_next_date(uint64_t alarm_id, char *out_date, size_t out_len)
{
	if (!out_date || out_len < 11) return -1;

	// 请求地址
	char url[128] = {0};
	const char *host_suffix = "";
	int device_mode = 0;
	if (lisa_kv_get_int(KV_KEY_DEVICE_MODE, &device_mode) != 0) {
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

	const char *token = get_lsc_jwt_token();
	if (token == NULL) {
		return -2;
	}
	size_t header_len = strlen(header_fmt) + strlen(token) + 1;
	if (s_listenai_date_headers) {
		lisa_mem_free(s_listenai_date_headers);
		s_listenai_date_headers = NULL;
	}

	s_listenai_date_headers = lisa_mem_calloc(1, header_len);
	if (!s_listenai_date_headers) {
		return -3;
	}
	snprintf(s_listenai_date_headers, header_len, header_fmt, token);

	// 请求体
	char body[128];
	int body_len = snprintf(body, sizeof(body), "{\"type\":\"custom\",\"id\":\"%llu\"}",
	                        (unsigned long long)alarm_id);
	if (body_len < 0 || body_len >= (int)sizeof(body)) {
		lisa_mem_free(s_listenai_date_headers);
		s_listenai_date_headers = NULL;
		return -4;
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
		return -9;
	}

	lisa_http_err_e err = lisa_http_perform(http);
	lisa_http_cleanup(http);

	if (s_listenai_date_headers) {
		lisa_mem_free(s_listenai_date_headers);
		s_listenai_date_headers = NULL;
	}

	if (err != LISA_HTTP_OK) {
		if (json) cJSON_Delete(json);
		return -10;
	}

	if (!json) {
		return -5;
	}

	char *json_str = cJSON_PrintUnformatted(json);
	if (json_str) {
		LISA_LOGI(TAG, "custom alarm next date response: alarm_id=%llu resp=%s",
		         (unsigned long long)alarm_id, json_str);
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

	strncpy(out_date, date->valuestring, out_len - 1);
	out_date[out_len - 1] = '\0';

	cJSON_Delete(json);
	return 0;
}

/* ==================== 云端闹钟同步 ==================== */

static cJSON *listenai_alarm_list_get(int *out_err)
{
	if (out_err) *out_err = -1;

	// 请求地址
	char url[128] = {0};
	const char *host_suffix = "";
	int device_mode = 0;
	if (lisa_kv_get_int(KV_KEY_DEVICE_MODE, &device_mode) != 0) {
		device_mode = 0;
	}
	if (device_mode == 1) {
		host_suffix = "staging-";
	} else if (device_mode == 2) {
		host_suffix = "integration-";
	}
	snprintf(url, sizeof(url), "http://%sapi.listenai.com/v1/alarm_clocks", host_suffix);

	// 请求头
	const char *token = get_lsc_jwt_token();
	if (token == NULL) {
		LISA_LOGE(TAG, "alarm_cloud_list_get: token is NULL");
		if (out_err) *out_err = -2;
		return NULL;
	}

	// 从 app_datas 获取 product_id 和 device_id
	struct app_datas *app_data = get_app_datas();
	if (!app_data) {
		LISA_LOGE(TAG, "alarm_cloud_list_get: app_data is NULL");
		if (out_err) *out_err = -3;
		return NULL;
	}

	const char *product_id = app_data->pid;
	const char *device_id = app_data->did;

	// 构建请求头：Content-Type + Authorization + x-product-id + x-device-id
	const char *header_fmt = "Content-Type: application/json\r\nAuthorization: Bearer %s\r\nx-product-id: %s\r\nx-device-id: %s";
	size_t header_len = strlen(header_fmt) + strlen(token) +
	                    strlen(product_id) + strlen(device_id) + 1;

	// 使用静态变量保存 headers
	if (s_alarm_sync_headers) {
		lisa_mem_free(s_alarm_sync_headers);
		s_alarm_sync_headers = NULL;
	}

	s_alarm_sync_headers = lisa_mem_calloc(1, header_len);
	if (!s_alarm_sync_headers) {
		LISA_LOGE(TAG, "alarm_cloud_list_get: failed to allocate headers");
		if (out_err) *out_err = -4;
		return NULL;
	}

	snprintf(s_alarm_sync_headers, header_len, header_fmt,
	         token, product_id, device_id);

	// ===== 打印请求信息 =====
	LISA_LOGI(TAG, "========== Alarm Sync Request ==========");
	LISA_LOGI(TAG, "URL: %s", url);
	LISA_LOGI(TAG, "Method: GET");
	LISA_LOGI(TAG, "Headers:");
	LISA_LOGI(TAG, "  Content-Type: application/json");
	LISA_LOGI(TAG, "  Authorization: Bearer %s", token);
	LISA_LOGI(TAG, "  x-product-id: %s", product_id);
	LISA_LOGI(TAG, "  x-device-id: %s", device_id);
	LISA_LOGI(TAG, "========================================");

	cJSON *json = NULL;
	lisa_http_request_t req = {
		.method = LISA_HTTP_GET,
		.url = (uint8_t *)url,
		.timeout = 10,
		.headers = (uint8_t *)alarm_sync_headers_cb,
		.on_data = listenai_date_json_on_data,
		.user = &json,
	};

	lisa_http_t *http = lisa_http_init(&req);
	if (!http) {
		LISA_LOGE(TAG, "alarm_cloud_list_get: lisa_http_init failed");
		lisa_mem_free(s_alarm_sync_headers);
		s_alarm_sync_headers = NULL;
		if (out_err) *out_err = -9;
		return NULL;
	}

	LISA_LOGI(TAG, "Sending HTTP request...");
	lisa_http_err_e err = lisa_http_perform(http);
	LISA_LOGI(TAG, "HTTP request completed, err=%d", err);

	lisa_http_cleanup(http);

	if (s_alarm_sync_headers) {
		lisa_mem_free(s_alarm_sync_headers);
		s_alarm_sync_headers = NULL;
	}

	if (err != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "alarm_cloud_list_get: HTTP request failed, err=%d", err);
		if (json) cJSON_Delete(json);
		if (out_err) *out_err = -10;
		return NULL;
	}

	// ===== 打印响应信息 =====
	LISA_LOGI(TAG, "========== Alarm Sync Response ==========");
	if (json) {
		char *json_str = cJSON_Print(json);
		if (json_str) {
			LISA_LOGI(TAG, "Response JSON: %s", json_str);
			lisa_mem_free(json_str);
		} else {
			LISA_LOGW(TAG, "Response JSON: (failed to print)");
		}
	} else {
		LISA_LOGW(TAG, "Response JSON: NULL");
	}
	LISA_LOGI(TAG, "=========================================");

	if (!json && out_err) {
		*out_err = -5;
	}
	return json;
}

static int alarm_parse_cloud_object(cJSON *item, alarm_object_t *alarm)
{
	if (!item || !alarm) return -1;

	memset(alarm, 0, sizeof(alarm_object_t));

	// 解析 id
	cJSON *id = cJSON_GetObjectItem(item, "id");
	if (!cJSON_IsNumber(id)) return -1;
	alarm->cloud_id = (uint64_t)id->valuedouble;

	// 解析 alarm_type
	cJSON *alarm_type = cJSON_GetObjectItem(item, "alarm_type");
	if (cJSON_IsString(alarm_type) && alarm_type->valuestring) {
		if (strcmp(alarm_type->valuestring, "ONCE") == 0) {
			alarm->trigger.type = ALARM_TRIG_ONCE;
		} else if (strcmp(alarm_type->valuestring, "DAILY") == 0) {
			alarm->trigger.type = ALARM_TRIG_DAILY;
		} else if (strcmp(alarm_type->valuestring, "WEEKLY") == 0) {
			alarm->trigger.type = ALARM_TRIG_WEEKLY;
		} else if (strcmp(alarm_type->valuestring, "WORKDAY") == 0) {
			alarm->trigger.type = ALARM_TRIG_WORKDAY;
		} else if (strcmp(alarm_type->valuestring, "WEEKEND") == 0) {
			alarm->trigger.type = ALARM_TRIG_WEEKEND;
		} else if (strcmp(alarm_type->valuestring, "MONTHLY") == 0) {
			alarm->trigger.type = ALARM_TRIG_MONTHLY;
		} else if (strcmp(alarm_type->valuestring, "YEARLY") == 0) {
			alarm->trigger.type = ALARM_TRIG_YEARLY;
		} else if (strcmp(alarm_type->valuestring, "CUSTOM") == 0) {
			alarm->trigger.type = ALARM_TRIG_CUSTOM;
		}
	}

	// 解析 calendar_type
	cJSON *calendar_type = cJSON_GetObjectItem(item, "calendar_type");
	if (cJSON_IsString(calendar_type) && calendar_type->valuestring) {
		if (strcmp(calendar_type->valuestring, "LUNAR") == 0) {
			alarm->calendar = ALARM_CAL_LUNAR;
		} else {
			alarm->calendar = ALARM_CAL_GREGORIAN;
		}
	}

	// 解析 time
	cJSON *time = cJSON_GetObjectItem(item, "time");
	if (cJSON_IsString(time) && time->valuestring) {
		alarm_time_parse_hms(time->valuestring,
		                    &alarm->trigger.hour,
		                    &alarm->trigger.minute,
		                    &alarm->trigger.second);
	}

	// 解析 date
	cJSON *date = cJSON_GetObjectItem(item, "date");
	if (cJSON_IsString(date) && date->valuestring && date->valuestring[0] != '\0') {
		alarm_time_parse_ymd(date->valuestring,
		                    &alarm->trigger.year,
		                    &alarm->trigger.month,
		                    &alarm->trigger.day);
	}

	// 解析 day_of_week
	cJSON *day_of_week = cJSON_GetObjectItem(item, "day_of_week");
	if (cJSON_IsNumber(day_of_week)) {
		alarm->trigger.day_of_week = (uint8_t)day_of_week->valueint;
	}

	// 解析 day_of_month
	cJSON *day_of_month = cJSON_GetObjectItem(item, "day_of_month");
	if (cJSON_IsNumber(day_of_month)) {
		alarm->trigger.day_of_month = (uint8_t)day_of_month->valueint;
	}

	// 解析 month_of_year
	cJSON *month_of_year = cJSON_GetObjectItem(item, "month_of_year");
	if (cJSON_IsNumber(month_of_year)) {
		alarm->trigger.month = (uint8_t)month_of_year->valueint;
	}

	// 解析 holiday_name
	cJSON *holiday_name = cJSON_GetObjectItem(item, "holiday_name");
	if (cJSON_IsString(holiday_name) && holiday_name->valuestring && holiday_name->valuestring[0] != '\0') {
		strncpy(alarm->trigger.holiday_name, holiday_name->valuestring,
		        sizeof(alarm->trigger.holiday_name) - 1);
	}

	// 解析 lunar_date
	cJSON *lunar_date = cJSON_GetObjectItem(item, "lunar_date");
	if (cJSON_IsString(lunar_date) && lunar_date->valuestring && lunar_date->valuestring[0] != '\0') {
		alarm_time_parse_lunar(lunar_date->valuestring,
		                      &alarm->trigger.lunar_month,
		                      &alarm->trigger.lunar_day);
	}

	// 解析 text
	cJSON *text = cJSON_GetObjectItem(item, "text");
	if (cJSON_IsString(text) && text->valuestring) {
		strncpy(alarm->text, text->valuestring, sizeof(alarm->text) - 1);
	}

	// 解析 snooze_enabled
	cJSON *snooze_enabled = cJSON_GetObjectItem(item, "snooze_enabled");
	if (cJSON_IsBool(snooze_enabled)) {
		alarm->snooze_enabled = cJSON_IsTrue(snooze_enabled);
	} else {
		alarm->snooze_enabled = true;  // 默认开启
	}

	// 解析 snooze_interval
	cJSON *snooze_interval = cJSON_GetObjectItem(item, "snooze_interval");
	if (cJSON_IsNumber(snooze_interval)) {
		alarm->snooze_interval = (uint8_t)snooze_interval->valueint;
	} else {
		alarm->snooze_interval = 5;  // 默认 5 分钟
	}

	// 解析 snooze_count
	cJSON *snooze_count = cJSON_GetObjectItem(item, "snooze_count");
	if (cJSON_IsNumber(snooze_count)) {
		alarm->snooze_count = (uint8_t)snooze_count->valueint;
	} else {
		alarm->snooze_count = 3;  // 默认 3 次
	}

	return 0;
}

int alarm_sync_from_cloud(void)
{
	LISA_LOGI(TAG, "========== Starting Alarm Sync from Cloud ==========");
	int err = 0;
	cJSON *json = listenai_alarm_list_get(&err);
	if (!json) {
		LISA_LOGE(TAG, "failed to get cloud alarm list, err=%d", err);
		return err;
	}

	cJSON *code = cJSON_GetObjectItem(json, "code");
	if (!cJSON_IsNumber(code) || code->valueint != 0) {
		LISA_LOGE(TAG, "cloud alarm list response error, code=%d",
		         cJSON_IsNumber(code) ? code->valueint : -1);
		cJSON_Delete(json);
		return -6;
	}

	cJSON *data = cJSON_GetObjectItem(json, "data");
	if (!cJSON_IsArray(data)) {
		LISA_LOGE(TAG, "cloud alarm list data is not array");
		cJSON_Delete(json);
		return -7;
	}

	// 获取本地所有闹钟
	int local_count = alarm_nvs_count();
	alarm_object_t *local_alarms = NULL;
	if (local_count > 0) {
		local_alarms = lisa_mem_calloc(local_count, sizeof(alarm_object_t));
		if (local_alarms) {
			alarm_nvs_get_all(local_alarms, local_count);
		}
	}

	// 标记本地闹钟是否在云端存在
	bool *local_exists = NULL;
	if (local_count > 0) {
		local_exists = lisa_mem_calloc(local_count, sizeof(bool));
	}

	int cloud_count = cJSON_GetArraySize(data);
	LISA_LOGI(TAG, "syncing alarms: local=%d, cloud=%d", local_count, cloud_count);

	// 遍历云端闹钟列表
	cJSON *item = NULL;
	cJSON_ArrayForEach(item, data) {
		alarm_object_t cloud_alarm;
		if (alarm_parse_cloud_object(item, &cloud_alarm) != 0) {
			LISA_LOGW(TAG, "failed to parse cloud alarm, skipping");
			continue;
		}

		// 查找本地是否有相同 cloud_id 的闹钟
		int local_idx = -1;
		for (int i = 0; i < local_count; i++) {
			if (local_alarms && local_alarms[i].cloud_id == cloud_alarm.cloud_id) {
				local_idx = i;
				break;
			}
		}

		if (local_idx >= 0 && local_exists) {
			// 本地存在，标记为存在
			local_exists[local_idx] = true;

			// 对比关键字段是否一致
			alarm_object_t *local = &local_alarms[local_idx];
			bool need_update = false;

			// 对比 alarm_type
			if (cloud_alarm.trigger.type != local->trigger.type) {
				need_update = true;
			}
			// 对比 time
			else if (cloud_alarm.trigger.hour != local->trigger.hour ||
			         cloud_alarm.trigger.minute != local->trigger.minute ||
			         cloud_alarm.trigger.second != local->trigger.second) {
				need_update = true;
			}
			// 对比 day_of_week (WEEKLY)
			else if (cloud_alarm.trigger.day_of_week != local->trigger.day_of_week) {
				need_update = true;
			}
			// 对比 text
			else if (strcmp(cloud_alarm.text, local->text) != 0) {
				need_update = true;
			}
			// 对比 snooze 配置
			else if (cloud_alarm.snooze_enabled != local->snooze_enabled ||
			         cloud_alarm.snooze_interval != local->snooze_interval ||
			         cloud_alarm.snooze_count != local->snooze_count) {
				need_update = true;
			}

			if (need_update) {
				LISA_LOGI(TAG, "alarm updated, cloud_id=%llu", (unsigned long long)cloud_alarm.cloud_id);
				// 删除旧的
				alarm_nvs_delete(cloud_alarm.cloud_id);
				// 重新计算触发时间
				if (cloud_alarm.trigger.type != ALARM_TRIG_CUSTOM &&
				    cloud_alarm.trigger.year == 0) {
					// 需要生成日期（custom 类型除外）
					alarm_construct_params_t params = {0};
					params.cloud_id = cloud_alarm.cloud_id;
					params.alarm_type = (cloud_alarm.trigger.type == ALARM_TRIG_ONCE) ? "ONCE" :
					                   (cloud_alarm.trigger.type == ALARM_TRIG_DAILY) ? "DAILY" :
					                   (cloud_alarm.trigger.type == ALARM_TRIG_WEEKLY) ? "WEEKLY" :
					                   (cloud_alarm.trigger.type == ALARM_TRIG_WORKDAY) ? "WORKDAY" :
					                   (cloud_alarm.trigger.type == ALARM_TRIG_WEEKEND) ? "WEEKEND" :
					                   (cloud_alarm.trigger.type == ALARM_TRIG_MONTHLY) ? "MONTHLY" :
					                   (cloud_alarm.trigger.type == ALARM_TRIG_YEARLY) ? "YEARLY" : "ONCE";
					params.time_str = lisa_mem_alloc(16);
					if (params.time_str) {
						snprintf((char *)params.time_str, 16, "%02d:%02d:%02d",
						        cloud_alarm.trigger.hour, cloud_alarm.trigger.minute, cloud_alarm.trigger.second);
						params.day_of_week = cloud_alarm.trigger.day_of_week;
						params.day_of_month = cloud_alarm.trigger.day_of_month;
						params.month_of_year = cloud_alarm.trigger.month;
						params.holiday_name = cloud_alarm.trigger.holiday_name;
						params.text = cloud_alarm.text;

						alarm_object_t new_alarm;
						if (alarm_construct(&params, &new_alarm) == 0) {
							alarm_nvs_add(&new_alarm);
						}
						lisa_mem_free((void *)params.time_str);
					}
				} else {
					// 有日期，直接生成时间戳
					cloud_alarm.alarm_id = alarm_time_obj_to_timestamp(&cloud_alarm);
					if (cloud_alarm.alarm_id > 0) {
						alarm_nvs_add(&cloud_alarm);
					}
				}
			}
		} else {
			// 本地不存在，创建新闹钟
			LISA_LOGI(TAG, "new alarm from cloud, cloud_id=%llu", (unsigned long long)cloud_alarm.cloud_id);

			if (cloud_alarm.trigger.type != ALARM_TRIG_CUSTOM &&
			    cloud_alarm.trigger.year == 0) {
				// 需要生成日期
				alarm_construct_params_t params = {0};
				params.cloud_id = cloud_alarm.cloud_id;
				params.alarm_type = (cloud_alarm.trigger.type == ALARM_TRIG_ONCE) ? "ONCE" :
				                   (cloud_alarm.trigger.type == ALARM_TRIG_DAILY) ? "DAILY" :
				                   (cloud_alarm.trigger.type == ALARM_TRIG_WEEKLY) ? "WEEKLY" :
				                   (cloud_alarm.trigger.type == ALARM_TRIG_WORKDAY) ? "WORKDAY" :
				                   (cloud_alarm.trigger.type == ALARM_TRIG_WEEKEND) ? "WEEKEND" :
				                   (cloud_alarm.trigger.type == ALARM_TRIG_MONTHLY) ? "MONTHLY" :
				                   (cloud_alarm.trigger.type == ALARM_TRIG_YEARLY) ? "YEARLY" : "ONCE";
				params.time_str = lisa_mem_alloc(16);
				if (params.time_str) {
					snprintf((char *)params.time_str, 16, "%02d:%02d:%02d",
					        cloud_alarm.trigger.hour, cloud_alarm.trigger.minute, cloud_alarm.trigger.second);
					params.day_of_week = cloud_alarm.trigger.day_of_week;
					params.day_of_month = cloud_alarm.trigger.day_of_month;
					params.month_of_year = cloud_alarm.trigger.month;
					params.holiday_name = cloud_alarm.trigger.holiday_name;
					params.text = cloud_alarm.text;

					alarm_object_t new_alarm;
					if (alarm_construct(&params, &new_alarm) == 0) {
						alarm_nvs_add(&new_alarm);
					}
					lisa_mem_free((void *)params.time_str);
				}
			} else {
				// 有日期，直接生成时间戳
				cloud_alarm.alarm_id = alarm_time_obj_to_timestamp(&cloud_alarm);
				if (cloud_alarm.alarm_id > 0) {
					alarm_nvs_add(&cloud_alarm);
				}
			}
		}
	}

	// 清除本地多余的闹钟（云端不存在的）
	for (int i = 0; i < local_count; i++) {
		if (local_exists && !local_exists[i] && local_alarms) {
			LISA_LOGI(TAG, "deleting local alarm not in cloud, cloud_id=%llu",
			         (unsigned long long)local_alarms[i].cloud_id);
			alarm_nvs_delete(local_alarms[i].cloud_id);
		}
	}

	// 清理
	if (local_alarms) lisa_mem_free(local_alarms);
	if (local_exists) lisa_mem_free(local_exists);
	cJSON_Delete(json);

	LISA_LOGI(TAG, "alarm sync completed");
	return 0;
}