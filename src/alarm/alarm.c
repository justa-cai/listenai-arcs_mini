#define TAG "alarm"

#include <stdint.h>
#include <stdio.h>
#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"
#include "string.h"
#include "utlist.h"

#include "time.h"
#include "math.h"

#include "lisa_typedef.h"
#include "lisa_mem.h"
#include "lisa_timer.h"
#include "lisa_log.h"
#include "lisa_mutex.h"
#include "lisa_kv.h"
#include "alarm.h"
#include "sys/time.h"

static struct ls_alarm *g_alarm_head = NULL;
static lisa_timer_t *g_alarm_timer = NULL;
static lisa_mutex_t *g_alarm_mutex = NULL;
static ls_alarm_user_callback_t g_alarm_user_callback = NULL;
int ls_alarm_count_get(void);

static void ls_alarm_timer_callback_handle(void *arg);

static void ls_alarm_timer_start(uint32_t timeout_s)
{
	LISA_LOGI(TAG, "ls_alarm_timer_start, timeout_s:%d s\r\n", timeout_s);

	timeout_s = timeout_s < 1 ? 1 : timeout_s;

	uint32_t timeout_ms = timeout_s * 1000;
	if (g_alarm_timer == NULL) {
		g_alarm_timer = lisa_timer_create(1000, ls_alarm_timer_callback_handle, NULL);
	}

	if (!lisa_timer_isactive(g_alarm_timer)) {
		timeout_ms = timeout_ms < 1000 ? 1000 : timeout_ms;
		lisa_timer_change_period(g_alarm_timer, timeout_ms);
		lisa_timer_start(g_alarm_timer);
		LISA_LOGI(TAG, "start new alarm timer, timeout:%dms\r\n", timeout_ms);
	} else {
		uint32_t remain = lisa_timer_remain_time(g_alarm_timer);
		if (remain > timeout_ms) {
			lisa_timer_change_period(g_alarm_timer, timeout_ms);
			lisa_timer_start(g_alarm_timer);
			LISA_LOGI(TAG, "alarm timer remain:%dms, new period:%dms\r\n", remain, timeout_ms);
		}
	}
}

static int ls_alarm_delete_key_from_nvs_keys_by_timestamp(uint64_t timestamp)
{
	int err;
	uint8_t *alarm_keys;
	int alarm_keys_len;
	uint8_t *new_alarm_keys;
	int new_alarm_keys_len;
	int alarm_key_cnt;

	err = lisa_kv_get_blob(NVS_ALARM_KEY, (uint8_t **)&alarm_keys, &alarm_keys_len);
	if (err) {
		return err;
	}
	LISA_LOGI(TAG, "alarm_keys_len:%d", alarm_keys_len);

	new_alarm_keys_len = alarm_keys_len;
	new_alarm_keys = lisa_mem_alloc(new_alarm_keys_len);
	if (new_alarm_keys == NULL) {
		lisa_mem_free(alarm_keys);
		return -1;
	}

	uint64_t *k1 = (uint64_t *)alarm_keys;
	uint64_t *k2 = (uint64_t *)new_alarm_keys;

	while (alarm_keys_len) {
		if (*k1 != timestamp) {
			*k2++ = *k1++;
		} else {
			k1++;
			new_alarm_keys_len -= sizeof(uint64_t);
		}
		alarm_keys_len -= sizeof(uint64_t);
	}

	LISA_LOGI(TAG, "new_alarm_keys_len:%d", new_alarm_keys_len);
	if (new_alarm_keys_len <= 0) {
		lisa_kv_del(NVS_ALARM_KEY);
	} else {
		err = lisa_kv_set_blob(NVS_ALARM_KEY, new_alarm_keys, new_alarm_keys_len);
	}

	lisa_mem_free(alarm_keys);
	lisa_mem_free(new_alarm_keys);

	return err;
}

static int ls_alarm_delete_from_nvs_by_key(const char *key)
{
	int err;

	err = lisa_kv_del(key);

	return err;
}

static int ls_alarm_delete_from_nvs_by_timestamp(uint64_t timestamp)
{
	int err;
	uint8_t timestamp_string[32] = {0};

	sprintf(timestamp_string, "%lld", timestamp);

	/* delete alarm blob first */
	err = ls_alarm_delete_from_nvs_by_key(timestamp_string);

	/* then delete the alarm key in alarm keys from nvs */
	err |= ls_alarm_delete_key_from_nvs_keys_by_timestamp(timestamp);

	return err;
}

static int ls_alarm_add_to_nvs_keys_by_timestamp(uint64_t timestamp)
{
	uint8_t *alarm_keys = NULL;
	int keys_size;
	int new_key_size;
	int cnt;
	int err;
	uint64_t key;
	uint8_t *new_alarm_keys = NULL;

	err = lisa_kv_get_blob(NVS_ALARM_KEY, (uint8_t **)&alarm_keys, &keys_size);
	if (err) {
		keys_size = 0;
	}

	new_key_size = keys_size + sizeof(uint64_t);
	new_alarm_keys = lisa_mem_alloc(new_key_size);
	if (new_alarm_keys == NULL) {
		lisa_mem_free(alarm_keys);
		return -1;
	}

	/* cpy old keys first */
	memcpy(new_alarm_keys, alarm_keys, keys_size);
	/* add new key */
	memcpy(new_alarm_keys + keys_size, &timestamp, sizeof(uint64_t));

	err = lisa_kv_set_blob(NVS_ALARM_KEY, (char *)new_alarm_keys, new_key_size);

	lisa_mem_free(new_alarm_keys);
	lisa_mem_free(alarm_keys);

	return err;
}

static int ls_alarm_add_to_nvs(uint64_t timestamp, const char *text)
{
	int err;

	struct ls_alarm_nvs *alarm_nvs = lisa_mem_alloc(sizeof(struct ls_alarm_nvs));
	if (alarm_nvs == NULL) {
		return -1;
	}

	uint8_t timestamp_string[32] = {0};
	sprintf(timestamp_string, "%lld", timestamp);

	memset(alarm_nvs, 0, sizeof(sizeof(struct ls_alarm_nvs)));

	alarm_nvs->timestamp = timestamp;
	alarm_nvs->text_len = text != NULL ? strlen(text) : 0;
	alarm_nvs->text_len = (alarm_nvs->text_len >= sizeof(alarm_nvs->text))
								  ? sizeof(alarm_nvs->text) - 1
								  : alarm_nvs->text_len;

	memcpy(alarm_nvs->text, text, alarm_nvs->text_len);

	LISA_LOGI(TAG, "ls alarm save to nvs, timestamp:%lld, text:%s", timestamp, text);

	/* save ls_alarm blob to nvs first */
	err = lisa_kv_set_blob(timestamp_string, (uint8_t *)alarm_nvs, sizeof(struct ls_alarm_nvs));

	/* then save the ls_alarm key to nvs keys */
	err |= ls_alarm_add_to_nvs_keys_by_timestamp(timestamp);

	lisa_mem_free(alarm_nvs);

	return err;
}

static int ls_alarm_update_in_nvs_by_key(const char *key, struct ls_alarm_nvs *alarm_nvs)
{
	int err;
	int out_len;
	struct ls_alarm_nvs *temp;

	if (key == NULL || alarm_nvs == NULL) {
		return -1;
	}

	err = lisa_kv_get_blob(key, (uint8_t **)&temp, &out_len);
	if (err) {
		/* the alarm not exist */
		return -1;
	}

	lisa_mem_free(temp);
	err = lisa_kv_set_blob(key, (char *)alarm_nvs, sizeof(struct ls_alarm_nvs));

	return err;
}

static int ls_alarm_update_in_nvs(struct ls_alarm_nvs *alarm_nvs)
{
	uint8_t timestamp_string[32] = {0};
	sprintf(timestamp_string, "%lld", alarm_nvs->timestamp);

	return ls_alarm_update_in_nvs_by_key(timestamp_string, alarm_nvs);
}

static int ls_alarm_update(struct ls_alarm *alarm, struct ls_alarm *new)
{
	int err;

	if (alarm == NULL) {
		return -1;
	}

	memset(alarm->text, 0, sizeof(alarm->text));
	strcat(alarm->text, new->text);

	struct ls_alarm_nvs *alarm_nvs = lisa_mem_alloc(sizeof(struct ls_alarm_nvs));
	if (alarm_nvs == NULL) {
		return -1;
	}

	memset(alarm_nvs, 0, sizeof(struct ls_alarm_nvs));
	alarm_nvs->timestamp = new->timestamp;
	strcat(alarm_nvs->text, new->text);

	err = ls_alarm_update_in_nvs(alarm_nvs);

	lisa_mem_free(alarm_nvs);

	return err;
}

static struct ls_alarm *ls_alarm_get_next_alarm(void)
{
	return g_alarm_head;
}

static int ls_alarm_cmp(struct ls_alarm *a1, struct ls_alarm *a2)
{
	return a1->timestamp - a2->timestamp;
}

static void ls_alarm_print_timestamp(uint64_t timestamp)
{
	struct tm _tm = {0};
	struct tm *ret_tm;

	ret_tm = localtime_r(&timestamp, &_tm);

	LISA_LOGI(TAG, "timestamp:%lld, local time:%04d年%02d月%02d日%02d时%02d分%02d秒", timestamp,
			ret_tm->tm_year + 1970, ret_tm->tm_mon + 1, ret_tm->tm_mday, ret_tm->tm_hour, ret_tm->tm_min, ret_tm->tm_sec);
}

static int ls_alarm_insert(struct ls_alarm *alarm)
{
	struct ls_alarm *node;
	struct ls_alarm *temp;
	struct timeval tm;

	if (alarm == NULL) {
		return -1;
	}

	temp = lisa_mem_alloc(sizeof(struct ls_alarm));
	if (temp == NULL) {
		return -1;
	}
	temp->timestamp = alarm->timestamp;

	lisa_mutex_lock(g_alarm_mutex, LISA_OS_WAIT_FOREVER);

	/* the new alarm node is exist or not */
	DL_SEARCH(g_alarm_head, node, temp, ls_alarm_cmp);
	if (node) {
		LISA_LOGI(TAG, "alarm is already exist, timestamp:%lld, old text:%s new text:%s\r\n",
				node->timestamp, node->text, alarm->text);
		/* the new alarm node is exist, update the text */
		int err = ls_alarm_update(node, alarm);
		lisa_mutex_unlock(g_alarm_mutex);
		lisa_mem_free(temp);
		return err;
	}
	lisa_mem_free(temp);

	int alarm_cnt = ls_alarm_count_get();
	LISA_LOGI(TAG, "current alarm cnt:%d", alarm_cnt);

	/* node is not exist, insert it to alarm list */
	LISA_LOGI(TAG, "insert new alarm, timestamp:%lld, text:%s\r\n", alarm->timestamp, alarm->text);
	ls_alarm_print_timestamp(alarm->timestamp);
	DL_INSERT_INORDER(g_alarm_head, alarm, ls_alarm_cmp);

	/* get current utc time */
	gettimeofday(&tm, NULL);
	uint64_t current_timestamp = tm.tv_sec;

	/* start the timer use first alarm node timestamp */
	node = ls_alarm_get_next_alarm();
	LISA_LOGI(TAG, "ls alarm insert, node->timestamp:%lld, current_timestamp:%lld", node->timestamp, current_timestamp);
	LISA_LOGI(TAG, "ls alarm insert, alarm->timestamp:%lld", alarm->timestamp);
	ls_alarm_timer_start(node->timestamp - current_timestamp);

	/* add alarm to nvs */
	ls_alarm_add_to_nvs(alarm->timestamp, alarm->text);

	lisa_mutex_unlock(g_alarm_mutex);

	return 0;
}

static int ls_alarm_delete(struct ls_alarm *alarm)
{
	int err = -1;

	if (alarm == NULL) {
		return -1;
	}

	/* remove tht alarm from nvs system first */
	err = ls_alarm_delete_from_nvs_by_timestamp(alarm->timestamp);

	DL_DELETE(g_alarm_head, alarm);

	lisa_mem_free(alarm);

	return err;
}

int ls_alarm_delete_by_timestamp(uint64_t timestamp)
{
	struct ls_alarm *node;
	struct ls_alarm *temp;
	int err = -1;

	temp = lisa_mem_alloc(sizeof(struct ls_alarm));
	if (temp == NULL) {
		return -1;
	}
	temp->timestamp = timestamp;

	lisa_mutex_lock(g_alarm_mutex, LISA_OS_WAIT_FOREVER);

	DL_SEARCH(g_alarm_head, node, temp, ls_alarm_cmp);
	if (node) {
		LISA_LOGI(TAG, "ls alarm delete, timestamp:%lld", timestamp);
		err = ls_alarm_delete(node);
	} else {
		LISA_LOGI(TAG, "ls alarm delete not found, timestamp:%lld", timestamp);
	}

	lisa_mutex_unlock(g_alarm_mutex);
	lisa_mem_free(temp);

	return err;
}

static uint64_t ls_alarm_convert_timestamp_from_string(const uint8_t *str)
{
	return 0;
}

static bool ls_alarm_timestamp_is_valid(uint64_t timestamp)
{
	struct timeval tm;
	gettimeofday(&tm, NULL);
	bool valid = timestamp > tm.tv_sec;
	if (!valid) {
		LISA_LOGI(TAG,
				"ls alarm timestamp is invalid, current timestamp:%lld, alarm timestamp:%lld",
				timestamp, tm.tv_sec);
	}

	return valid;
}

static int ls_alarm_insert_inner_by_timestamp(uint64_t timestamp, const uint8_t *text, ls_alarm_callbacks_t cb)
{
	if (!ls_alarm_timestamp_is_valid(timestamp)) {
		return -1;
	}

	struct ls_alarm *alarm = lisa_mem_alloc(sizeof(struct ls_alarm));

	if (alarm == NULL) {
		return -1;
	}

	memset(alarm, 0, sizeof(struct ls_alarm));
	alarm->cb = cb;
	alarm->timestamp = timestamp;
	if (text != NULL) {
		int cpy_len = strlen(text);
		if (cpy_len > (sizeof(alarm->text) - 1)) {
			cpy_len = sizeof(alarm->text) - 1;
		}

		memcpy(alarm->text, text, cpy_len);
	}

	if (ls_alarm_insert(alarm) != 0) {
		lisa_mem_free(alarm);
		return -1;
	}

	return 0;
}

static void ls_alarm_test_callback(struct ls_alarm *alarm, void *data)
{
	if (g_alarm_user_callback) {
		g_alarm_user_callback(alarm->timestamp, alarm->text);
	}
}

int ls_alarm_insert_by_timestamp(uint64_t timestamp, const uint8_t *text)
{
	return ls_alarm_insert_inner_by_timestamp(timestamp, text, ls_alarm_test_callback);
}

int ls_alarm_insert_by_timestamp_string(
		uint8_t *ts_string, const uint8_t *text)
{
	if (ts_string == NULL) {
		return -1;
	}

	uint64_t timestamp = ls_alarm_convert_timestamp_from_string(ts_string);
	return ls_alarm_insert_by_timestamp(timestamp, text);
}

static void ls_alarm_timer_callback_handle(void *arg)
{
	struct ls_alarm *node, *temp;
	struct timeval tm;

	gettimeofday(&tm, NULL);

	LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, current time: %lld", tm.tv_sec);
	node = ls_alarm_get_next_alarm();
	while (node != NULL) {
		if (tm.tv_sec >= node->timestamp) {
			if (node->cb) {
				node->cb(node, NULL);
			}
			ls_alarm_delete(node);
		} else {
			/* no new node timeout, break loop */
			break;
		}
		node = ls_alarm_get_next_alarm();
	}

	node = ls_alarm_get_next_alarm();
	if (node) {
		LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, node->timestamp:%lld", node->timestamp);
		if (node->timestamp > tm.tv_sec) {
			uint32_t next_timer_period = node->timestamp - tm.tv_sec;
			ls_alarm_timer_start(next_timer_period);
		} else {
			LISA_LOGW(TAG, "ls_alarm_timer_callback_handle, node->timestamp invalid");
		}
	} else {
		LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, node is null");
	}
}

static int ls_alarm_insert_by_alarm_nvs(struct ls_alarm_nvs *alarm_nvs)
{
	return ls_alarm_insert_by_timestamp(
			alarm_nvs->timestamp, alarm_nvs->text);
}

static void ls_alarm_init_from_nvs_key(const char *key)
{
	
}

static void ls_alarm_init_from_nvs_by_timestamp(uint64_t timestamp)
{
	int err;
	struct ls_alarm_nvs *alarm_nvs = NULL;
	int out_len = 0;
	uint8_t timestamp_string[32] = {0};

	sprintf(timestamp_string, "%lld", timestamp);

	err = lisa_kv_get_blob(timestamp_string, (uint8_t **)&alarm_nvs, &out_len);
	if (err) {
		LISA_LOGI(TAG, "the blob of key %s not found, delete the key", timestamp_string);
		/* the key`s blob is not exist, delete the key */
		ls_alarm_delete_key_from_nvs_keys_by_timestamp(timestamp);
		return;
	}

	alarm_nvs->text[alarm_nvs->text_len] = 0;

	LISA_LOGI(TAG, "ls alarm from nvs, timestamp: %lld, text:%s", timestamp, alarm_nvs);

	if (!ls_alarm_timestamp_is_valid(alarm_nvs->timestamp)) {
		/* the alarm timestamp is not valid, delete it from nvs */
		LISA_LOGI(TAG, "the blob of key %s is not valid, delete the blob and key", timestamp_string);
		/* delete alarm blob first */
		err = ls_alarm_delete_from_nvs_by_key(timestamp_string);
		/* then delete the alarm key in alarm keys from nvs */
		err |= ls_alarm_delete_key_from_nvs_keys_by_timestamp(timestamp);
	} else {
		err = ls_alarm_insert_by_alarm_nvs(alarm_nvs);
	}

	lisa_mem_free(alarm_nvs);
}

static void ls_alarm_init_from_nvs(void)
{
	uint8_t *alarm_keys = NULL;
	int keys_size;
	int cnt;
	int err;
	uint64_t *key;

	err = lisa_kv_get_blob(NVS_ALARM_KEY, (uint8_t **)&alarm_keys, &keys_size);
	if (err) {
		LISA_LOGI(TAG, "ls alarm keys is not exist");
		return;
	}

	cnt = keys_size / sizeof(uint64_t);
	LISA_LOGI(TAG, "ls alarm nvs count: %d", cnt);

	key = (uint64_t *)alarm_keys;
	while (cnt) {
		LISA_LOGI(TAG, "ls alarm nvs key: %lld", *key);
		ls_alarm_init_from_nvs_by_timestamp(*key);
		cnt--;
		key++;
	}

	lisa_mem_free(alarm_keys);
}

void ls_alarm_init(ls_alarm_user_callback_t cb)
{
	if (g_alarm_mutex == NULL) {
		g_alarm_mutex = lisa_mutex_create();
	}

	g_alarm_user_callback = cb;

	ls_alarm_init_from_nvs();
}

int ls_alarm_count_get(void)
{
	int cnt = 0;
	struct ls_alarm *node;

	DL_COUNT(g_alarm_head, node, cnt);

	return cnt;
}

struct ls_alarm *ls_alarm_get(void){
	return g_alarm_head;
}
