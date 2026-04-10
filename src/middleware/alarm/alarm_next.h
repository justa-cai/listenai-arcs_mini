#ifndef __ALARM_NEXT_H__
#define __ALARM_NEXT_H__

#include <stdint.h>
#include <time.h>
#include "alarm_nvs.h"

/**
 * @brief 计算循环闹钟的下一次触发时间，并更新 alarm 的年月日/时分秒
 *
 * @param alarm  闹钟对象（会被更新触发日期）
 * @param now_ts 当前时间戳（秒）
 * @return 下一次触发时间戳，失败/无下次返回 0
 */
uint64_t alarm_calc_next_trigger(alarm_object_t *alarm, time_t now_ts);

/**
 * @brief 调用 ListenAI 日期转换服务，将节日名/农历日期转换为公历日期
 *
 * @param type     类型："holiday" 或 "lunar"
 * @param date_name 节日名或农历日期（如 "圣诞节" / "01-01"）
 * @param out_date 输出缓冲区，返回格式 "YYYY-MM-DD"
 * @param out_len  输出缓冲区长度（需至少 11 字节含结尾 \0）
 * @return 0 成功；非 0 失败
 */
int listenai_date_transition(const char *type, const char *date_name, char *out_date, size_t out_len);

/**
 * @brief 查询指定日期是否工作日
 *
 * @param date_name 日期（格式 "YYYY-MM-DD"，如 "2026-03-22"）
 * @param out_is_work_day 输出是否工作日
 * @return 0 成功；非 0 失败
 */
int listenai_date_is_workday(const char *date_name, bool *out_is_work_day);

/**
 * @brief 计算循环闹钟的首次触发日期
 *
 * 根据触发类型，从当前时间开始计算最近的符合条件的日期，
 * 并更新 alarm 中的 year/month/day 字段。
 *
 * @param alarm 闹钟对象（需已设置 hour/minute/second 和 trigger.type）
 * @param day_of_week 周几 (1-7)，WEEKLY 类型使用，-1 表示不使用
 * @param day_of_month 每月几号 (1-31)，MONTHLY 类型使用，-1 表示不使用
 * @param month_of_year 每年几月 (1-12)，YEARLY 类型使用，-1 表示不使用
 * @return 0 成功，非 0 失败
 */
int alarm_calc_first_trigger(alarm_object_t *alarm,
                              int day_of_week,
                              int day_of_month,
                              int month_of_year);

#endif
