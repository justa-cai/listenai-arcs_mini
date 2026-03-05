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
#include "listen_system.h"
#include "sys/time.h"
#include "alarm_store.h"
#include "voice_msg.h"

static struct ls_alarm *g_alarm_head = NULL;
static lisa_timer_t *g_alarm_timer = NULL;
static lisa_mutex_t *g_alarm_mutex = NULL;
static ls_alarm_user_callback_t g_alarm_user_callback = NULL;

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
	struct tm _tm = {0};
	struct tm *ret_tm;

	ret_tm = localtime_r(&timestamp, &_tm);

	LISA_LOGI(TAG, "timestamp:%lld, local time:%04d年%02d月%02d日%02d时%02d分%02d秒", timestamp,
			ret_tm->tm_year + 1970, ret_tm->tm_mon + 1, ret_tm->tm_mday, ret_tm->tm_hour, ret_tm->tm_min, ret_tm->tm_sec);
}

static int get_network_time(struct tm *network_time, time_t *network_timestamp)
{
	struct timeval tv;

	/* Prefer SNTP-synced calendar; fallback to gettimeofday */
	if (ls_sys_get_time(&tv) != 0) {
		if (gettimeofday(&tv, NULL) < 0) {
			return -1;
		}
	}

	/* Use the same conversion helpers as listen_system to avoid platform mismatches */
	long int ts = (long int)tv.tv_sec;
	if (network_timestamp) {
		*network_timestamp = (time_t)ts;
	}
	if (network_time) {
		ls_sys_get_tmtime(&ts, network_time);
	}

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
		LISA_LOGI(TAG, "Clearing alarm with timestamp:%lld", node->timestamp);
		alarm_store_delete_obj_by_timestamp(node->timestamp);
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
	struct ls_alarm *node, *temp;
	struct tm now;
	time_t now_ts;

	get_network_time(&now, &now_ts);

	LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, current time: %lld", now_ts);
	node = ls_alarm_get();
	// 基于闹钟类型（单次/循环）处理闹钟对象
	while (node != NULL) {
		if (now_ts >= node->timestamp) {
			uint64_t fired_ts = node->timestamp;
			if (node->cb) {
				node->cb(node, NULL);
			}
			// 1. 通过 alarm_store_find_by_id 查询闹钟对象
			const alarm_object_t *alarm_obj = alarm_store_find_by_id(node->timestamp);
			if (alarm_obj) {
				LISA_LOGI(TAG, "alarm fired, id %llu type=%u cal=%u",
						(unsigned long long)alarm_obj->alarm_id,
						(unsigned)alarm_obj->trigger.type,
						(unsigned)alarm_obj->calendar);
				if (alarm_obj->trigger.type == ALARM_TRIG_ONCE) {
					// 单次闹钟，直接删除
					alarm_store_delete_obj_by_timestamp(node->timestamp);
					if (ls_alarm_delete(node) == 0) {
						voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
					}
				} else {
					// 循环闹钟，计算下次触发时间
					alarm_object_t next_alarm = *alarm_obj;
					uint64_t next_ts = alarm_calc_next_trigger(&next_alarm, now_ts);
					LISA_LOGI(TAG, "alarm next_ts=%llu (now=%llu)",
							(unsigned long long)next_ts, (unsigned long long)now_ts);

					if (next_ts > now_ts) {
						next_alarm.alarm_id = next_ts;
						int create_ret = alarm_store_create_obj(&next_alarm, NULL, 0);
						ls_alarm_insert_by_timestamp(next_alarm.alarm_id, next_alarm.text);
						LISA_LOGI(TAG, "alarm create_ret=%d next_id=%llu",
								create_ret, (unsigned long long)next_alarm.alarm_id);
						if (create_ret != 0) {
							LISA_LOGE(TAG, "failed to create next alarm, next_ts=%llu",
									(unsigned long long)next_ts);
						}
						// 3. 删除旧实例
						alarm_store_delete_obj_by_timestamp(node->timestamp);
						ls_alarm_delete(node);
					} else {
						// 没有下次触发，直接删除
						alarm_store_delete_obj_by_timestamp(node->timestamp);
						if (ls_alarm_delete(node) == 0) {
							voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
						}
					}
				}
			} else {
				// 查不到对象，直接删
				if (ls_alarm_delete(node) == 0) {
					voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
				}
			}
		} else {
			break;
		}
		node = ls_alarm_get();
	}

	node = ls_alarm_get();
	if (node) {
		LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, node->timestamp:%lld", node->timestamp);
		if (node->timestamp > now_ts) {
			uint64_t next_timer_period = (uint64_t)node->timestamp - (uint64_t)now_ts;
			ls_alarm_timer_start(next_timer_period);
		} else {
			LISA_LOGW(TAG, "ls_alarm_timer_callback_handle, node->timestamp invalid");
		}
	} else {
		LISA_LOGI(TAG, "ls_alarm_timer_callback_handle, node is null");
	}
}

/* ==================== 新增闹钟 ==================== */


static int ls_alarm_update(struct ls_alarm *alarm, struct ls_alarm *new)
{
	if (alarm == NULL || new == NULL) {
		return -1;
	}

	memset(alarm->text, 0, sizeof(alarm->text));
	strcat(alarm->text, new->text);

	int ret = alarm_store_update_obj_by_timestamp(new->timestamp, new->text);

	return ret;
}


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
		LISA_LOGI(TAG, "alarm is already exist, timestamp:%lld, old text:%s new text:%s\r\n",
				node->timestamp, node->text, alarm->text);

		// 新增闹钟有提示文本时，再更新闹钟文本
		int err = 0;
		if (alarm->text[0] != 0) {
			err = ls_alarm_update(node, alarm);
		}
		
		lisa_mutex_unlock(g_alarm_mutex);
		lisa_mem_free(temp);
		return err;
	}
	lisa_mem_free(temp);

	LISA_LOGI(TAG, "insert new alarm, timestamp:%lld, text:%s\r\n", alarm->timestamp, alarm->text);
	ls_alarm_print_timestamp(alarm->timestamp);

	// 插入新闹钟
	DL_INSERT_INORDER(g_alarm_head, alarm, ls_alarm_cmp);

	// 创建定时器（基于当前时间）并打印剩余触发时间
	struct tm network_time = {0};
	time_t network_time_ts = 0;
	get_network_time(&network_time, &network_time_ts);
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

static int ls_alarm_insert_inner_by_timestamp(uint64_t timestamp, const uint8_t *text, ls_alarm_callbacks_t cb)
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


/* ==================== 初始化 ==================== */


void ls_alarm_init(ls_alarm_user_callback_t cb)
{
	if (g_alarm_mutex == NULL) {
		g_alarm_mutex = lisa_mutex_create();
	}

	g_alarm_user_callback = cb;

	alarm_store_init();

	int alarm_cnt = ls_alarm_count_get();
	LISA_LOGI(TAG, "alarm init complete, count:%d", alarm_cnt);
}
