#ifndef __ALARM_TIME_UTILS_H__
#define __ALARM_TIME_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>
#include "alarm_types.h"

/* 编译时检查：确保 time_t 至少是 8 字节（64位），避免 2038 年问题 */
#ifndef __cplusplus
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        _Static_assert(sizeof(time_t) >= 8, "time_t must be at least 64-bit to avoid Y2038 issue");
    #endif
#endif


/**
 * @brief 获取网络同步时间戳（秒）
 *
 * 优先使用 SNTP 同步的网络时间，失败则使用系统时间。
 * 这是获取当前时间的推荐接口。
 *
 * @return 当前网络时间戳（秒），失败返回 -1
 */
int64_t alarm_time_get_network_timestamp(void);

/* ==================== 时间戳与日期时间转换接口 ==================== */

/**
 * @brief 时间戳转换为日期时间字段
 *
 * 将 Unix 时间戳转换为年月日时分秒，基于北京时间（UTC+8）
 *
 * @param timestamp Unix 时间戳（秒）
 * @param year 输出：年份（如 2026）
 * @param month 输出：月份（1-12）
 * @param day 输出：日期（1-31）
 * @param hour 输出：小时（0-23）
 * @param minute 输出：分钟（0-59）
 * @param second 输出：秒数（0-59）
 * @return 0 成功，非 0 失败
 */
int alarm_time_timestamp_to_date(int64_t timestamp,
                            uint16_t *year, uint8_t *month, uint8_t *day,
                            uint8_t *hour, uint8_t *minute, uint8_t *second);

/**
 * @brief 日期时间字段转换为时间戳
 *
 * 将年月日时分秒转换为 Unix 时间戳，基于北京时间（UTC+8）
 *
 * @param year 年份（如 2026）
 * @param month 月份（1-12）
 * @param day 日期（1-31）
 * @param hour 小时（0-23）
 * @param minute 分钟（0-59）
 * @param second 秒数（0-59）
 * @return Unix 时间戳，失败返回 0
 */
uint64_t alarm_time_date_to_timestamp(uint16_t year, uint8_t month, uint8_t day,
                                 uint8_t hour, uint8_t minute, uint8_t second);

/**
 * @brief 从闹钟对象提取时间戳
 *
 * 将 alarm_object_t 的触发时间字段转换为 Unix 时间戳
 *
 * @param alarm 闹钟对象
 * @return Unix 时间戳，失败返回 0
 */
uint64_t alarm_time_obj_to_timestamp(const alarm_object_t *alarm);

/**
 * @brief 获取星期几（不依赖 struct tm）
 *
 * 从 Unix 时间戳计算星期几
 *
 * @param timestamp Unix 时间戳（秒）
 * @return 星期几：0=周日, 1=周一, ..., 6=周六, 失败返回 -1
 */
int alarm_time_get_weekday(int64_t timestamp);

/* ==================== 时间字符串解析 ==================== */

/**
 * @brief 解析时间字符串
 *
 * 支持格式：
 * - "HH:mm:ss" （如 "14:30:00"）
 * - "HH:mm" （如 "14:30"，秒默认为 0）
 *
 * @param time_str 时间字符串
 * @param hour 输出：小时（0-23）
 * @param minute 输出：分钟（0-59）
 * @param second 输出：秒数（0-59）
 * @return 0 成功，非 0 失败
 */
int alarm_time_parse_hms(const char *time_str, uint8_t *hour, uint8_t *minute, uint8_t *second);

/**
 * @brief 解析日期字符串
 *
 * 格式："YYYY-MM-DD" （如 "2026-03-19"）
 *
 * @param date_str 日期字符串
 * @param year 输出：年份（≥1970）
 * @param month 输出：月份（1-12）
 * @param day 输出：日期（1-31）
 * @return 0 成功，非 0 失败
 */
int alarm_time_parse_ymd(const char *date_str, uint16_t *year, uint8_t *month, uint8_t *day);

/**
 * @brief 解析农历日期字符串
 *
 * 格式："MM-DD" （如 "01-15" 表示正月十五）
 *
 * @param lunar_str 农历日期字符串
 * @param month 输出：月份（1-12）
 * @param day 输出：日期（1-31）
 * @return 0 成功，非 0 失败
 */
int alarm_time_parse_lunar(const char *lunar_str, uint8_t *month, uint8_t *day);

#ifdef __cplusplus
}
#endif

#endif // __ALARM_TIME_UTILS_H__