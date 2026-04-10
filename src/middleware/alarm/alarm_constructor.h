#ifndef __ALARM_CONSTRUCTOR_H__
#define __ALARM_CONSTRUCTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "alarm_types.h"

/**
 * @brief 闹钟构造参数
 *
 * 用于从上层传递参数到构造层
 */
typedef struct {
    uint64_t cloud_id;             // 云端闹钟 ID
    const char *action;            // CREATE / DELETE / UPDATE / QUERY
    const char *alarm_type;        // ONCE / DAILY / WEEKLY / WORKDAY / WEEKEND / MONTHLY / YEARLY
    const char *holiday_name;      // 节日名称
    const char *calendar_type;     // GREGORIAN / LUNAR
    const char *time_str;          // HH:mm:ss
    const char *date_str;          // YYYY-MM-DD
    const char *lunar_date_str;    // MM-DD (农历)
    int day_of_week;               // 1-7 (WEEKLY)
    int day_of_month;              // 1-31 (MONTHLY)
    int month_of_year;             // 1-12 (YEARLY)
    const char *text;              // 提醒文本

    /* Snooze 稍后提醒参数 */
    int snooze_enabled;            // -1: 未设置, 0: 禁用, 1: 启用
    int snooze_interval;           // 提醒间隔（分钟）, -1: 未设置
    int snooze_count;              // 提醒次数, -1: 未设置
} alarm_construct_params_t;

/**
 * @brief 从参数构造闹钟对象
 *
 * 该函数会：
 * 1. 解析时间和日期字符串
 * 2. 根据触发类型验证参数完整性
 * 3. 如果是过期闹钟，自动计算最近的未来触发时间
 * 4. 生成 alarm_id (timestamp)
 *
 * @param params 构造参数
 * @param alarm 输出的闹钟对象
 * @return 0 成功，非 0 失败
 */
int alarm_construct(const alarm_construct_params_t *params, alarm_object_t *alarm);


#ifdef __cplusplus
}
#endif

#endif // __ALARM_CONSTRUCTOR_H__