#ifndef __ALARM_TYPES_H__
#define __ALARM_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "alarm.h"

/* 历法类型 */
typedef enum {
    ALARM_CAL_GREGORIAN = 0,
    ALARM_CAL_LUNAR = 1,
} alarm_calendar_t;

/* 触发类型 */
typedef enum {
    ALARM_TRIG_ONCE = 0,
    ALARM_TRIG_DAILY,
    ALARM_TRIG_WEEKLY,
    ALARM_TRIG_WORKDAY,
    ALARM_TRIG_WEEKEND,
    ALARM_TRIG_MONTHLY,
    ALARM_TRIG_YEARLY,
    ALARM_TRIG_CUSTOM,
} alarm_trigger_type_t;

/* 触发规则 */
typedef struct {
    alarm_trigger_type_t type;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    uint16_t year;           // ONCE: 触发年份
    uint8_t month;           // ONCE/YEARLY: 触发月份
    uint8_t day;             // ONCE/YEARLY/MONTHLY: 触发日期
    uint8_t day_of_week;     // WEEKLY: 周几 (1-7)
    uint8_t day_of_month;    // MONTHLY: 每月几号
    uint8_t month_of_year;   // YEARLY: 每年几月
    uint8_t lunar_month;     // LUNAR: 农历月
    uint8_t lunar_day;       // LUNAR: 农历日
    char holiday_name[24];   // HOLIDAY: 节日名称
} alarm_trigger_t;

/* 闹钟对象（领域模型）*/
typedef struct {
    uint64_t alarm_id;       // 时间戳，用于本地调度和 NVS key
    uint64_t cloud_id;       // 云端 ID，用于云端操作
    alarm_calendar_t calendar;
    alarm_trigger_t trigger;
    char text[LS_ALARM_TEXT_MAX_LEN];

    /* Snooze 稍后提醒配置 */
    bool snooze_enabled;     // 是否开启稍后提醒（默认 true）
    uint8_t snooze_interval; // 提醒间隔（分钟，默认 5）
    uint8_t snooze_count;    // 最大提醒次数（默认 3）
} alarm_object_t;

#ifdef __cplusplus
}
#endif

#endif // __ALARM_TYPES_H__