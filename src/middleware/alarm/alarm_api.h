#ifndef __ALARM_API_H__
#define __ALARM_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file alarm_api.h
 * @brief 闹钟相关的外部 API 封装（日期转换、工作日查询等）
 *
 * 封装 listenai 日期服务 API，提供农历转换、节假日查询、工作日判断等功能
 */

/**
 * @brief 日期转换
 *
 * 调用 listenai API 进行日期转换（农历转公历、节假日查询等）
 *
 * @param type 转换类型：lunar（农历）, holiday（节假日）
 * @param date_name 日期名称或日期字符串
 * @param out_date 输出公历日期（格式：YYYY-MM-DD），可为 NULL
 * @param out_len out_date 缓冲区长度，至少 11 字节
 * @return 0 成功，非 0 失败
 *
 * @example
 * // 农历正月初一转公历
 * char gregorian_date[16];
 * listenai_date_transition("lunar", "01-01", gregorian_date, sizeof(gregorian_date));
 *
 * // 查询春节日期
 * listenai_date_transition("holiday", "春节", gregorian_date, sizeof(gregorian_date));
 */
int listenai_date_transition(const char *type, const char *date_name, char *out_date, size_t out_len);

/**
 * @brief 查询是否为工作日
 *
 * 调用 listenai API 查询指定日期是否为法定工作日
 *
 * @param date_name 日期字符串（格式：YYYY-MM-DD，如 "2026-03-22"）
 * @param out_is_work_day 输出：是否为工作日
 * @return 0 成功，非 0 失败
 *
 * @note 法定工作日定义：
 *       - 周一至周五（除法定节假日）
 *       - 法定调休工作日（周末调休上班）
 */
int listenai_date_is_workday(const char *date_name, bool *out_is_work_day);

/**
 * @brief 查询自定义闹钟的下次触发日期
 *
 * 调用 listenai API 查询 custom 类型闹钟的下次触发日期
 *
 * @param alarm_id 闹钟云端 ID
 * @param out_date 输出公历日期（格式：YYYY-MM-DD），至少 11 字节
 * @param out_len out_date 缓冲区长度
 * @return 0 成功，非 0 失败
 *
 * @example
 * char next_date[16];
 * listenai_custom_alarm_next_date(4181, next_date, sizeof(next_date));
 */
int listenai_custom_alarm_next_date(uint64_t alarm_id, char *out_date, size_t out_len);

/**
 * @brief 从云端同步闹钟列表
 *
 * 调用 listenai API 获取云端闹钟列表，并与本地 NVS 闹钟对比同步
 * 确保端侧闹钟对象与云端保持一致
 *
 * @return 0 成功，非 0 失败
 *
 * @note 在系统启动时调用，会自动：
 *       - 删除本地多余的闹钟
 *       - 创建云端新增的闹钟
 *       - 更新不一致的闹钟
 */
int alarm_sync_from_cloud(void);

#ifdef __cplusplus
}
#endif

#endif // __ALARM_API_H__