#include "alarm_next.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "alarm_nvs.h"
#include "alarm_time_utils.h"
#include "alarm_api.h"
#include "lisa_log.h"

#define TAG "alarm_next"

/* ==================== 辅助工具 ==================== */

static int alarm_update_trigger_from_timestamp(alarm_object_t *alarm, uint64_t timestamp)
{
	if (!alarm || timestamp == 0) {
		return -1;
	}

	return alarm_time_timestamp_to_date((int64_t)timestamp,
						   &alarm->trigger.year, &alarm->trigger.month, &alarm->trigger.day,
						   &alarm->trigger.hour, &alarm->trigger.minute, &alarm->trigger.second);
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

/* ==================== 触发时间计算 - 各类型策略函数 ==================== */
static uint64_t calc_next_holiday(alarm_object_t *alarm, time_t now_ts);

static uint64_t calc_next_daily(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	uint64_t next = base;
	while (next <= (uint64_t)now_ts) {
		next += 24 * 60 * 60;
	}
	// 只更新日期字段，保留用户设置的时间
	alarm_time_timestamp_to_date((int64_t)next,
	                            &alarm->trigger.year, &alarm->trigger.month, &alarm->trigger.day,
	                            NULL, NULL, NULL);
	return next;
}

static uint64_t calc_next_weekly(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	uint64_t next = base;
	while (next <= (uint64_t)now_ts) {
		next += 7 * 24 * 60 * 60;
	}
	// 只更新日期字段，保留用户设置的时间
	alarm_time_timestamp_to_date((int64_t)next,
	                            &alarm->trigger.year, &alarm->trigger.month, &alarm->trigger.day,
	                            NULL, NULL, NULL);
	return next;
}

static uint64_t calc_next_by_workday(alarm_object_t *alarm, time_t now_ts, uint64_t base, bool want_workday)
{
	uint64_t next = base;
	while (1) {
		next += 24 * 60 * 60;
		alarm->trigger.day += 1;
		char date_str[16] = {0};
		snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
		         alarm->trigger.year, alarm->trigger.month, alarm->trigger.day);
		bool is_work_day = true;
		if (listenai_date_is_workday(date_str, &is_work_day) != 0) {
			LISA_LOGE(TAG, "date check failed: %s", date_str);
			return 0;
		}
		if (is_work_day == want_workday) {
			break;
		}
	}
	// 只更新日期字段，保留用户设置的时间
	alarm_time_timestamp_to_date((int64_t)next,
	                            &alarm->trigger.year, &alarm->trigger.month, &alarm->trigger.day,
	                            NULL, NULL, NULL);
	return next;
}

static uint64_t calc_next_workday(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	return calc_next_by_workday(alarm, now_ts, base, true);
}

static uint64_t calc_next_weekend(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	return calc_next_by_workday(alarm, now_ts, base, false);
}

static uint64_t calc_next_monthly(alarm_object_t *alarm, time_t now_ts, uint64_t base)
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
		next = alarm_time_obj_to_timestamp(&temp);
	}
	// 只更新日期字段，保留用户设置的时间
	alarm->trigger.year = temp.trigger.year;
	alarm->trigger.month = temp.trigger.month;
	alarm->trigger.day = temp.trigger.day;
	return next;
}

static uint64_t calc_next_yearly(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	// 如果是节假日类型，使用 calc_next_holiday
	if (alarm->trigger.holiday_name[0] != '\0') {
		return calc_next_holiday(alarm, now_ts);
	}

	uint8_t desired_day = alarm->trigger.day;
	if (alarm->trigger.day_of_month > 0) {
		desired_day = alarm->trigger.day_of_month;
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
		next = alarm_time_obj_to_timestamp(&temp);
	}
	// 只更新日期字段，保留用户设置的时间
	alarm->trigger.year = temp.trigger.year;
	alarm->trigger.month = temp.trigger.month;
	alarm->trigger.day = temp.trigger.day;
	return next;
}

static uint64_t calc_next_custom(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	// Custom 类型闹钟：如果当前时间戳已过期，调用 API 查询下一个日期
	if (base > (uint64_t)now_ts) {
		// 未过期，直接返回
		return base;
	}

	// 已过期，查询云端获取下一个日期
	char next_date[16] = {0};
	int api_ret = listenai_custom_alarm_next_date(alarm->cloud_id, next_date, sizeof(next_date));
	if (api_ret != 0) {
		LISA_LOGE(TAG, "failed to query custom alarm next date, cloud_id=%llu, ret=%d",
		          (unsigned long long)alarm->cloud_id, api_ret);
		return 0;
	}

	LISA_LOGI(TAG, "custom alarm next date from API: %s", next_date);

	// 解析日期
	uint16_t y = 0;
	uint8_t m = 0, d = 0;
	if (alarm_time_parse_ymd(next_date, &y, &m, &d) != 0) {
		LISA_LOGE(TAG, "failed to parse next date: %s", next_date);
		return 0;
	}

	// 更新闹钟对象
	alarm->trigger.year = y;
	alarm->trigger.month = m;
	alarm->trigger.day = d;

	// 重新计算时间戳
	uint64_t new_alarm_id = alarm_time_obj_to_timestamp(alarm);

	if (new_alarm_id <= (uint64_t)now_ts) {
		LISA_LOGW(TAG, "custom alarm next date still expired, cloud_id=%llu, new_alarm_id=%llu",
		          (unsigned long long)alarm->cloud_id, (unsigned long long)new_alarm_id);
		return 0;
	}

	return new_alarm_id;
}

static uint64_t calc_next_holiday(alarm_object_t *alarm, time_t now_ts)
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
	// 只更新日期字段，保留用户设置的时间
	alarm->trigger.year = (uint16_t)year;
	alarm->trigger.month = (uint8_t)month;
	alarm->trigger.day = (uint8_t)day;
	uint64_t next = alarm_time_obj_to_timestamp(alarm);
	if (next <= (uint64_t)now_ts) {
		LISA_LOGW(TAG, "holiday next not in future, date=%s now=%lld",
			  gregorian_date, (long long)now_ts);
		return 0;
	}
	return next;
}

/* ==================== 农历触发时间计算 ==================== */

static uint64_t calc_next_lunar_monthly(alarm_object_t *alarm, time_t now_ts, uint64_t base)
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
		uint64_t next = alarm_time_obj_to_timestamp(&temp);
		if (next > (uint64_t)now_ts) {
			// 只更新日期字段，保留用户设置的时间
			alarm->trigger.year = temp.trigger.year;
			alarm->trigger.month = temp.trigger.month;
			alarm->trigger.day = temp.trigger.day;
			alarm->trigger.lunar_month = temp.trigger.lunar_month;
			alarm->trigger.lunar_day = temp.trigger.lunar_day;
			return next;
		}
	}
	LISA_LOGW(TAG, "lunar monthly next not in future, now=%lld", (long long)now_ts);
	return 0;
}

static uint64_t calc_next_lunar_yearly(alarm_object_t *alarm, time_t now_ts)
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
	// 只更新日期字段，保留用户设置的时间
	alarm->trigger.year = (uint16_t)year;
	alarm->trigger.month = (uint8_t)month;
	alarm->trigger.day = (uint8_t)day;
	uint64_t next = alarm_time_obj_to_timestamp(alarm);
	if (next <= (uint64_t)now_ts) {
		LISA_LOGW(TAG, "lunar yearly next not in future, date=%s now=%lld",
			  gregorian_date, (long long)now_ts);
		return 0;
	}
	return next;
}

static uint64_t alarm_calc_next_trigger_lunar(alarm_object_t *alarm, time_t now_ts, uint64_t base)
{
	if (!alarm) {
		return 0;
	}

	switch (alarm->trigger.type) {
	case ALARM_TRIG_DAILY:
		return calc_next_daily(alarm, now_ts, base);
	case ALARM_TRIG_WEEKLY:
		return calc_next_weekly(alarm, now_ts, base);
	case ALARM_TRIG_MONTHLY:
		return calc_next_lunar_monthly(alarm, now_ts, base);
	case ALARM_TRIG_YEARLY:
		return calc_next_lunar_yearly(alarm, now_ts);
	default:
		LISA_LOGW(TAG, "lunar trigger type not supported: %d", alarm->trigger.type);
		return 0;
	}
}

/* ==================== 公开接口 - 计算下次触发时间 ==================== */

uint64_t alarm_calc_next_trigger(alarm_object_t *alarm, time_t now_ts)
{
	if (!alarm) {
		return 0;
	}

	uint64_t base = alarm_time_obj_to_timestamp(alarm);
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
		return calc_next_daily(alarm, now_ts, base);
	case ALARM_TRIG_WEEKLY:
		return calc_next_weekly(alarm, now_ts, base);
	case ALARM_TRIG_WORKDAY:
		return calc_next_workday(alarm, now_ts, base);
	case ALARM_TRIG_WEEKEND:
		return calc_next_weekend(alarm, now_ts, base);
	case ALARM_TRIG_MONTHLY:
		return calc_next_monthly(alarm, now_ts, base);
	case ALARM_TRIG_YEARLY:
		return calc_next_yearly(alarm, now_ts, base);
	case ALARM_TRIG_CUSTOM:
		return calc_next_custom(alarm, now_ts, base);
	default:
		return 0;
	}
}

/* ==================== 公开接口 - 计算首次触发时间 ==================== */

int alarm_calc_first_trigger(alarm_object_t *alarm,
                              int day_of_week,
                              int day_of_month,
                              int month_of_year)
{
	if (!alarm) {
		return -1;
	}

	// 获取当前网络时间
	time_t now_ts = (time_t)alarm_time_get_network_timestamp();
	if (now_ts < 0) {
		LISA_LOGE(TAG, "failed to get network time");
		return -1;
	}

	// 获取当前日期字段
	uint16_t cur_year;
	uint8_t cur_month, cur_day, cur_hour, cur_min, cur_sec;
	if (alarm_time_timestamp_to_date(now_ts, &cur_year, &cur_month, &cur_day,
						   &cur_hour, &cur_min, &cur_sec) != 0) {
		LISA_LOGE(TAG, "failed to extract datetime");
		return -1;
	}

	// 根据类型计算首次触发日期
	switch (alarm->trigger.type) {
	case ALARM_TRIG_DAILY:
	{
		// 每日闹钟：设置今天的日期，检查时间是否已过
		alarm->trigger.year = cur_year;
		alarm->trigger.month = cur_month;
		alarm->trigger.day = cur_day;

		// 构造今天目标时间的时间戳
		uint64_t target_ts = alarm_time_date_to_timestamp(cur_year, cur_month, cur_day,
												alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

		if (target_ts > (uint64_t)now_ts) {
			// 今天还没过，直接使用
			alarm->alarm_id = target_ts;
			return 0;
		}
		// 今天已过，计算明天
		uint64_t next_ts = target_ts + 24 * 60 * 60;

		// 更新日期字段，但保留用户设置的时间
		alarm_time_timestamp_to_date((int64_t)next_ts,
		                            &alarm->trigger.year, &alarm->trigger.month, &alarm->trigger.day,
		                            NULL, NULL, NULL);
		alarm->alarm_id = next_ts;
		return 0;
	}

	case ALARM_TRIG_WORKDAY:
	{
		// 法定工作日：设置今天的日期，检查今天是否是工作日且时间未过
		alarm->trigger.year = cur_year;
		alarm->trigger.month = cur_month;
		alarm->trigger.day = cur_day;

		// 构造今天目标时间的时间戳
		uint64_t target_ts = alarm_time_date_to_timestamp(cur_year, cur_month, cur_day,
												alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

		// 检查今天是否是工作日
		char date_str[16] = {0};
		snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", cur_year, cur_month, cur_day);
		bool is_work_day = false;
		if (listenai_date_is_workday(date_str, &is_work_day) == 0 && is_work_day && target_ts > (uint64_t)now_ts) {
			// 今天是工作日且时间未过，直接使用
			alarm->alarm_id = target_ts;
			return 0;
		}

		// 今天不是工作日或时间已过，使用 alarm_calc_next_trigger 计算下一个工作日
		alarm->alarm_id = target_ts; // 设置临时时间戳
		uint64_t next_ts = alarm_calc_next_trigger(alarm, now_ts);
		if (next_ts == 0) {
			return -1;
		}
		alarm->alarm_id = next_ts;
		return 0;
	}

	case ALARM_TRIG_WEEKEND:
	{
		// 法定休息日：设置今天的日期，检查今天是否是休息日且时间未过
		alarm->trigger.year = cur_year;
		alarm->trigger.month = cur_month;
		alarm->trigger.day = cur_day;

		// 构造今天目标时间的时间戳
		uint64_t target_ts = alarm_time_date_to_timestamp(cur_year, cur_month, cur_day,
												alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

		// 检查今天是否是法定休息日
		char date_str[16] = {0};
		snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", cur_year, cur_month, cur_day);
		bool is_work_day = true;
		if (listenai_date_is_workday(date_str, &is_work_day) == 0 && !is_work_day && target_ts > (uint64_t)now_ts) {
			// 今天是法定休息日且时间未过，直接使用
			alarm->alarm_id = target_ts;
			return 0;
		}

		// 今天不是法定休息日或时间已过，使用 alarm_calc_next_trigger 计算下一个休息日
		alarm->alarm_id = target_ts; // 设置临时时间戳
		uint64_t next_ts = alarm_calc_next_trigger(alarm, now_ts);
		if (next_ts == 0) {
			return -1;
		}
		alarm->alarm_id = next_ts;
		return 0;
	}

	case ALARM_TRIG_MONTHLY:
	{
		// 月循环：设置当月的目标日期，检查是否已过
		if (day_of_month < 1 || day_of_month > 31) {
			LISA_LOGE(TAG, "MONTHLY alarm missing day_of_month");
			return -1;
		}
		alarm->trigger.day_of_month = (uint8_t)day_of_month;

		// 设置当前年月和目标日期
		alarm->trigger.year = cur_year;
		alarm->trigger.month = cur_month;
		int max_day = days_in_month(alarm->trigger.year, alarm->trigger.month);
		alarm->trigger.day = (day_of_month > max_day) ? (uint8_t)max_day : (uint8_t)day_of_month;

		// 构造目标时间的时间戳
		uint64_t target_ts = alarm_time_date_to_timestamp(alarm->trigger.year, alarm->trigger.month, alarm->trigger.day,
												alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

		if (target_ts > (uint64_t)now_ts) {
			// 本月还没过，直接使用
			alarm->alarm_id = target_ts;
			return 0;
		}

		// 本月已过，计算下个月
		alarm->alarm_id = target_ts; // 设置临时时间戳
		uint64_t next_ts = alarm_calc_next_trigger(alarm, now_ts);
		if (next_ts == 0) {
			return -1;
		}
		alarm->alarm_id = next_ts;
		return 0;
	}

	case ALARM_TRIG_YEARLY:
	{
		// 年循环：检查是否为节假日类型
		if (alarm->trigger.holiday_name[0] != '\0') {
			// 节假日类型：调用接口计算下次触发日期
			char gregorian_date[16] = {0};
			if (listenai_date_transition("holiday", alarm->trigger.holiday_name,
					    gregorian_date, sizeof(gregorian_date)) != 0) {
				LISA_LOGE(TAG, "YEARLY holiday transition failed, name=%s", alarm->trigger.holiday_name);
				return -1;
			}
			unsigned int year = 0, month = 0, day = 0;
			if (sscanf(gregorian_date, "%u-%u-%u", &year, &month, &day) != 3) {
				LISA_LOGE(TAG, "YEARLY holiday transition parse failed, date=%s", gregorian_date);
				return -1;
			}

			// 设置节假日对应的公历日期
			alarm->trigger.year = (uint16_t)year;
			alarm->trigger.month = (uint8_t)month;
			alarm->trigger.day = (uint8_t)day;

			// 构造目标时间的时间戳
			uint64_t target_ts = alarm_time_date_to_timestamp(alarm->trigger.year, alarm->trigger.month, alarm->trigger.day,
													alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

			if (target_ts > (uint64_t)now_ts) {
				// 今年的节假日还没过，直接使用
				alarm->alarm_id = target_ts;
				return 0;
			}

			// 今年的节假日已过，计算明年的触发时间
			alarm->alarm_id = target_ts; // 设置临时时间戳
			uint64_t next_ts = alarm_calc_next_trigger(alarm, now_ts);
			if (next_ts == 0) {
				return -1;
			}
			alarm->alarm_id = next_ts;
			return 0;
		}

		// 非节假日类型：需要 month_of_year 和 day_of_month
		if (month_of_year < 1 || month_of_year > 12) {
			LISA_LOGE(TAG, "YEARLY alarm missing month_of_year");
			return -1;
		}
		if (day_of_month < 1 || day_of_month > 31) {
			LISA_LOGE(TAG, "YEARLY alarm missing day_of_month");
			return -1;
		}
		alarm->trigger.month = (uint8_t)month_of_year;
		alarm->trigger.day_of_month = (uint8_t)day_of_month;

		// 设置当前年和目标月日
		alarm->trigger.year = cur_year;
		int max_day = days_in_month(alarm->trigger.year, alarm->trigger.month);
		alarm->trigger.day = (day_of_month > max_day) ? (uint8_t)max_day : (uint8_t)day_of_month;

		// 构造目标时间的时间戳
		uint64_t target_ts = alarm_time_date_to_timestamp(alarm->trigger.year, alarm->trigger.month, alarm->trigger.day,
												alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

		if (target_ts > (uint64_t)now_ts) {
			// 今年还没过，直接使用
			alarm->alarm_id = target_ts;
			return 0;
		}

		// 今年已过，计算明年
		alarm->alarm_id = target_ts; // 设置临时时间戳
		uint64_t next_ts = alarm_calc_next_trigger(alarm, now_ts);
		if (next_ts == 0) {
			return -1;
		}
		alarm->alarm_id = next_ts;
		return 0;
	}

	case ALARM_TRIG_WEEKLY:
	{
		// WEEKLY ：找到下一个目标星期几
		if (day_of_week < 1 || day_of_week > 7) {
			LISA_LOGE(TAG, "WEEKLY alarm missing day_of_week");
			return -1;
		}
		alarm->trigger.day_of_week = (uint8_t)day_of_week;

		// 设置当前日期和时间
		alarm->trigger.year = cur_year;
		alarm->trigger.month = cur_month;
		alarm->trigger.day = cur_day;


		// 目标星期几：1=周一 ... 7=周日
		// alarm_time_get_weekday: 0=周日 ... 6=周六
		int target_wday = (day_of_week == 7) ? 0 : day_of_week;
		int current_wday = alarm_time_get_weekday(now_ts);

		// 计算需要增加的天数
		int days_ahead = target_wday - current_wday;
		if (days_ahead < 0) {
			days_ahead += 7;
		}

		// 如果目标就是今天，检查时间是否已过
		if (days_ahead == 0) {
			// 构造今天目标时间的时间戳进行比较
			uint64_t target_ts = alarm_time_date_to_timestamp(cur_year, cur_month, cur_day,
													alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

			if (target_ts > (uint64_t)now_ts) {
				// 今天还没过，直接使用
				alarm->alarm_id = target_ts;
				return 0;
			}
			// 今天已过，设置为下周同一天
			days_ahead = 7;
		}

		// 增加天数，但保留用户设置的时间
		uint64_t target_ts = alarm_time_date_to_timestamp(cur_year, cur_month, cur_day,
		                                                  alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);
		uint64_t next_ts = target_ts + (uint64_t)days_ahead * 24 * 60 * 60;

		// 更新日期字段
		alarm_time_timestamp_to_date((int64_t)next_ts,
		                            &alarm->trigger.year, &alarm->trigger.month, &alarm->trigger.day,
		                            NULL, NULL, NULL);  // 不更新时间字段，保留用户设置
		alarm->alarm_id = next_ts;
		return 0;
	}

	case ALARM_TRIG_ONCE:
		// 不需要计算首次触发日期
		return 0;

	default:
		return -1;
	}
}