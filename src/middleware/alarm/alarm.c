#define TAG "alarm"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
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
#include "alarm_next.h"
#include "alarm.h"
#include "alarm_handler.h"
#include "listen_system.h"
#include "sys/time.h"
#include "alarm_time_utils.h"
#include "alarm_nvs.h"
#include "voice_msg.h"

static struct ls_alarm *g_alarm_head = NULL;
static lisa_timer_t *g_alarm_timer = NULL;
static lisa_mutex_t *g_alarm_mutex = NULL;

#define ALARM_TIMER_MAX_MS (24U * 60U * 60U * 1000U)

static void ls_alarm_timer_callback_handle(struct lisa_timer *timer);
static int ls_alarm_delete(struct ls_alarm *alarm);

/* ==================== 工具函数  ==================== */

struct ls_alarm *ls_alarm_get(void)
{
	return g_alarm_head;
}

int ls_alarm_count_get(void)
{
	int cnt = 0;
	struct ls_alarm *node;

	DL_COUNT(g_alarm_head, node, cnt);

	return cnt;
}

static int ls_alarm_cmp(struct ls_alarm *a1, struct ls_alarm *a2)
{
	return a1->timestamp - a2->timestamp;
}

static void ls_alarm_print_timestamp(uint64_t timestamp)
{
	uint16_t year;
	uint8_t month, day, hour, min, sec;

	if (alarm_time_timestamp_to_date(timestamp, &year, &month, &day, &hour, &min, &sec) == 0) {
		LISA_LOGI(TAG, "timestamp:%lld, local time:%04d年%02d月%02d日%02d时%02d分%02d秒",
				  (long long)timestamp, year, month, day, hour, min, sec);
	}
}


static bool ls_alarm_timestamp_is_valid(uint64_t timestamp)
{
	int64_t current_ts = alarm_time_get_network_timestamp();
	if (current_ts < 0) {
		LISA_LOGE(TAG, "failed to get network time");
		return false;
	}

	bool valid = timestamp > (uint64_t)current_ts;
	if (!valid) {
		LISA_LOGI(TAG,
				"ls alarm timestamp is invalid, current timestamp:%lld, alarm timestamp:%lld",
				(long long)current_ts, (long long)timestamp);
	}

	return valid;
}


/* ==================== 删除闹钟 ==================== */


static int ls_alarm_delete(struct ls_alarm *alarm)
{
	int err = 0;

	if (alarm == NULL) {
		return -1;
	}

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

int ls_alarm_clear_all(void)
{
	lisa_mutex_lock(g_alarm_mutex, LISA_OS_WAIT_FOREVER);

	struct ls_alarm *node, *tmp;
	DL_FOREACH_SAFE(g_alarm_head, node, tmp) {
		LISA_LOGI(TAG, "Clearing alarm with cloud_id=%llu, timestamp:%lld",
		          (unsigned long long)node->cloud_id, (long long)node->timestamp);
		alarm_nvs_delete(node->cloud_id);
		DL_DELETE(g_alarm_head, node);
		lisa_mem_free(node);
	}

	g_alarm_head = NULL;

	if (g_alarm_timer != NULL) {
		lisa_timer_stop(g_alarm_timer);
	}

	lisa_mutex_unlock(g_alarm_mutex);

	LISA_LOGI(TAG, "All alarms cleared");
	return 0;
}


/* ==================== 闹钟触发回调 ==================== */

static void ls_alarm_timer_start(uint64_t timeout_s)
{
	LISA_LOGI(TAG, "ls_alarm_timer_start, timeout_s:%llu s\r\n", (unsigned long long)timeout_s);

	timeout_s = timeout_s < 1 ? 1 : timeout_s;

	uint64_t timeout_ms64 = (uint64_t)timeout_s * 1000U;
	uint32_t timeout_ms = timeout_ms64 > ALARM_TIMER_MAX_MS ? ALARM_TIMER_MAX_MS : (uint32_t)timeout_ms64;
	if (timeout_ms64 > ALARM_TIMER_MAX_MS) {
		LISA_LOGI(TAG, "alarm timer capped: requested=%llus capped=%ums",
			  (unsigned long long)timeout_s, timeout_ms);
	}
	if (g_alarm_timer == NULL) {
		g_alarm_timer = lisa_timer_create(timeout_ms, ls_alarm_timer_callback_handle, NULL);
	}

	// 如果有更短时长的闹钟则更新更短时长
	if (!lisa_timer_isactive(g_alarm_timer)) {
		timeout_ms = timeout_ms < 2000 ? 2000 : timeout_ms;
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

static void ls_alarm_timer_callback_handle(struct lisa_timer *timer)
{
	struct ls_alarm *node;
	time_t now_ts;

	now_ts = (time_t)alarm_time_get_network_timestamp();

	LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, current time: %lld", now_ts);
	node = ls_alarm_get();

	// 处理触发的闹钟
	while (node != NULL && now_ts >= node->timestamp) {
		uint64_t fired_ts = node->timestamp;
		uint64_t cloud_id = node->cloud_id;

		ls_alarm_delete_by_timestamp(fired_ts);
		if (alarm_process_triggered_async(fired_ts, cloud_id, (int64_t)now_ts) != 0) {
			LISA_LOGE(TAG, "failed to submit alarm trigger, timestamp:%lld, cloud_id:%llu",
				  (long long)fired_ts, (unsigned long long)cloud_id);
		}
		node = ls_alarm_get();
	}

	// 启动下一个定时器
	node = ls_alarm_get();
	if (node) {
		LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, next timestamp:%lld", node->timestamp);
		if (node->timestamp > now_ts) {
			uint64_t next_timer_period = (uint64_t)node->timestamp - (uint64_t)now_ts;
			ls_alarm_timer_start(next_timer_period);
		} else {
			LISA_LOGW(TAG, "ls_alarm_timer_callback_handle, next timestamp invalid");
		}
	} else {
		LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, no more alarms");
	}
}

/* ==================== 新增闹钟 ==================== */


static int ls_alarm_insert(struct ls_alarm *alarm)
{
	struct ls_alarm *node;
	struct ls_alarm *temp;

	if (alarm == NULL) {
		return -1;
	}

	temp = lisa_mem_alloc(sizeof(struct ls_alarm));
	if (temp == NULL) {
		return -1;
	}
	temp->timestamp = alarm->timestamp;

	lisa_mutex_lock(g_alarm_mutex, LISA_OS_WAIT_FOREVER);

	// 检查新增闹钟是否已存在
	DL_SEARCH(g_alarm_head, node, temp, ls_alarm_cmp);
	if (node) {
		LISA_LOGW(TAG, "alarm already exist, timestamp:%lld, skip insert", node->timestamp);
		lisa_mutex_unlock(g_alarm_mutex);
		lisa_mem_free(temp);
		return 0; // 已存在，返回成功
	}
	lisa_mem_free(temp);

	LISA_LOGI(TAG, "insert new alarm, timestamp:%lld, cloud_id:%llu, text:%s\r\n",
	          alarm->timestamp, (unsigned long long)alarm->cloud_id, alarm->text);
	ls_alarm_print_timestamp(alarm->timestamp);

	// 插入新闹钟
	DL_INSERT_INORDER(g_alarm_head, alarm, ls_alarm_cmp);

	// 创建定时器（基于当前时间）并打印剩余触发时间
	time_t network_time_ts = 0;
	network_time_ts = (time_t)alarm_time_get_network_timestamp();
	time_t current_timestamp = network_time_ts;

	/* 使用最早的闹钟（链表头）启动定时器 */
	node = ls_alarm_get();
	if (node) {
		uint64_t remain_s = (node->timestamp > current_timestamp) ? (node->timestamp - current_timestamp) : 0;
		ls_alarm_timer_start(remain_s);

		long long hh = (long long)(remain_s / 3600);
		long long mm = (long long)((remain_s % 3600) / 60);
		long long ss = (long long)(remain_s % 60);

		LISA_LOGI(TAG, "ls alarm insert, next:%lld, now:%lld, remain:%llds (%02lld:%02lld:%02lld)",
			  node->timestamp, (long long)current_timestamp, (long long)remain_s, hh, mm, ss);
	} else {
		LISA_LOGW(TAG, "ls alarm insert, node is null after insert");
	}

	LISA_LOGI(TAG, "ls alarm insert, alarm->timestamp:%lld", alarm->timestamp);

	lisa_mutex_unlock(g_alarm_mutex);

	int alarm_cnt = ls_alarm_count_get();
	LISA_LOGI(TAG, "current alarm cnt:%d", alarm_cnt);

	return 0;
}

static int ls_alarm_insert_inner_by_timestamp(uint64_t timestamp, uint64_t cloud_id, const uint8_t *text, ls_alarm_callbacks_t cb)
{
	// 检查时间戳是否过期
	if (!ls_alarm_timestamp_is_valid(timestamp)) {
		return -1;
	}

	// 构造 ls_alarm 对象
	struct ls_alarm *alarm = lisa_mem_alloc(sizeof(struct ls_alarm));
	if (alarm == NULL) {
		return -1;
	}
	memset(alarm, 0, sizeof(struct ls_alarm));
	alarm->cb = cb;
	alarm->timestamp = timestamp;
	alarm->cloud_id = cloud_id;
	if (text != NULL) {
		int cpy_len = strlen(text);
		if (cpy_len > (sizeof(alarm->text) - 1)) {
			cpy_len = sizeof(alarm->text) - 1;
		}
		memcpy(alarm->text, text, cpy_len);
	}

	// 插入闹钟链表
	if (ls_alarm_insert(alarm) != 0) {
		lisa_mem_free(alarm);
		return -1;
	}

	return 0;
}

int ls_alarm_insert_by_timestamp(uint64_t timestamp, uint64_t cloud_id, const uint8_t *text)
{
	return ls_alarm_insert_inner_by_timestamp(timestamp, cloud_id, text, NULL);
}


/* ==================== 初始化 ==================== */


void ls_alarm_init(ls_alarm_user_callback_t cb)
{
	if (g_alarm_mutex == NULL) {
		g_alarm_mutex = lisa_mutex_create();
	}

	// 初始化处理模块
	alarm_handler_init(cb);

	alarm_nvs_init();

	int alarm_cnt = ls_alarm_count_get();
	LISA_LOGI(TAG, "alarm init complete, count:%d", alarm_cnt);
}
