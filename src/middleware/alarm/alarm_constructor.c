#define TAG "alarm_constructor"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#include "lisa_log.h"
#include "lisa_mem.h"
#include "listen_system.h"
#include "cJSON.h"

#include "alarm_constructor.h"
#include "alarm_next.h"
#include "alarm_time_utils.h"
#include "alarm_api.h"

/* ==================== 闹钟类型解析 ==================== */

static alarm_calendar_t parse_calendar_type(const char *calendar_type)
{
    if (calendar_type && strcmp(calendar_type, "LUNAR") == 0) {
        return ALARM_CAL_LUNAR;
    }
    return ALARM_CAL_GREGORIAN;
}

static alarm_trigger_type_t parse_trigger_type(const char *alarm_type, const char *holiday_name)
{
    if (!alarm_type) {
        return ALARM_TRIG_ONCE;
    }
    if (strcmp(alarm_type, "ONCE") == 0) {
        return ALARM_TRIG_ONCE;
    }
    if (strcmp(alarm_type, "DAILY") == 0) {
        return ALARM_TRIG_DAILY;
    }
    if (strcmp(alarm_type, "WEEKLY") == 0) {
        return ALARM_TRIG_WEEKLY;
    }
    if (strcmp(alarm_type, "WORKDAY") == 0) {
        return ALARM_TRIG_WORKDAY;
    }
    if (strcmp(alarm_type, "WEEKEND") == 0) {
        return ALARM_TRIG_WEEKEND;
    }
    if (strcmp(alarm_type, "MONTHLY") == 0) {
        return ALARM_TRIG_MONTHLY;
    }
    if (strcmp(alarm_type, "YEARLY") == 0) {
        return ALARM_TRIG_YEARLY;
    }
    if (strcmp(alarm_type, "CUSTOM") == 0) {
        return ALARM_TRIG_CUSTOM;
    }
    return ALARM_TRIG_ONCE;
}

/* ==================== 公开接口实现 ==================== */

int alarm_construct(const alarm_construct_params_t *params, alarm_object_t *alarm)
{
    if (!params || !alarm) {
        return -1;
    }

    memset(alarm, 0, sizeof(*alarm));
    alarm->cloud_id = params->cloud_id;
    alarm->calendar = parse_calendar_type(params->calendar_type);
    alarm->trigger.type = parse_trigger_type(params->alarm_type, params->holiday_name);

    // 1. 解析时间（必选）
    if (!params->time_str || params->time_str[0] == '\0') {
        LISA_LOGE(TAG, "time missing");
        
        return -1;
    }
    if (alarm_time_parse_hms(params->time_str, &alarm->trigger.hour,
                        &alarm->trigger.minute, &alarm->trigger.second) != 0) {
        LISA_LOGE(TAG, "invalid time format: %s", params->time_str);
        
        return -1;
    }

    // 2. 解析日期
    bool date_set = false;
    if (params->date_str) {
        uint16_t y = 0;
        uint8_t m = 0, d = 0;
        if (alarm_time_parse_ymd(params->date_str, &y, &m, &d) == 0) {
            alarm->trigger.year = y;
            alarm->trigger.month = m;
            alarm->trigger.day = d;
            date_set = true;
        }
    }

    // 3. 如果没有日期，根据类型构造
    if (!date_set) {
        // CUSTOM 类型必须有 date 参数
        if (alarm->trigger.type == ALARM_TRIG_CUSTOM) {
            LISA_LOGE(TAG, "CUSTOM alarm requires date parameter");
            return -1;
        }
        // 农历
        if (alarm->calendar == ALARM_CAL_LUNAR) {
            if (!params->lunar_date_str || params->lunar_date_str[0] == '\0') {
                LISA_LOGE(TAG, "lunar_date missing");
                
                return -1;
            }
            char gregorian_date[16] = {0};
            if (listenai_date_transition("lunar", params->lunar_date_str,
                                         gregorian_date, sizeof(gregorian_date)) != 0) {
                LISA_LOGE(TAG, "lunar transition failed");
                
                return -1;
            }
            uint16_t y = 0;
            uint8_t m = 0, d = 0;
            if (alarm_time_parse_ymd(gregorian_date, &y, &m, &d) != 0) {
                LISA_LOGE(TAG, "invalid lunar transition date: %s", gregorian_date);

                return -1;
            }
            alarm->trigger.year = y;
            alarm->trigger.month = m;
            alarm->trigger.day = d;
            date_set = true;
        }
        // 循环闹钟（DAILY/WEEKLY/MONTHLY/YEARLY/WORKDAY/WEEKEND）
        else if (alarm->trigger.type != ALARM_TRIG_ONCE) {
            // 先设置 holiday_name（YEARLY 类型可能需要）
            if (params->holiday_name && params->holiday_name[0] != '\0') {
                strncpy(alarm->trigger.holiday_name, params->holiday_name,
                        sizeof(alarm->trigger.holiday_name) - 1);
                alarm->trigger.holiday_name[sizeof(alarm->trigger.holiday_name) - 1] = '\0';
            }

            int ret = alarm_calc_first_trigger(alarm,
                                                 params->day_of_week,
                                                 params->day_of_month,
                                                 params->month_of_year);
            if (ret != 0) {
                LISA_LOGE(TAG, "calc first trigger date failed, type=%d", alarm->trigger.type);
                
                return -1;
            }
            date_set = true;
        }
        // 单次闹钟（默认今天）
        else {
            int64_t current_ts = alarm_time_get_network_timestamp();
            if (current_ts < 0) {
                LISA_LOGE(TAG, "get current time failed");
                
                return -1;
            }
            if (alarm_time_timestamp_to_date(current_ts,
                                   &alarm->trigger.year, &alarm->trigger.month, &alarm->trigger.day,
                                   NULL, NULL, NULL) != 0) {
                LISA_LOGE(TAG, "extract datetime failed");
                
                return -1;
            }
            date_set = true;
        }
    }

    // 4. 生成 alarm_id (timestamp)
    alarm->alarm_id = alarm_time_obj_to_timestamp(alarm);
    if (alarm->alarm_id == 0) {
        LISA_LOGE(TAG, "invalid alarm datetime");
        
        return -1;
    }

    // 5. 检查是否过期，如果是循环闹钟则计算下次时间
    int now_ts = alarm_time_get_network_timestamp();
    if (now_ts > 0 && (int)alarm->alarm_id <= now_ts) {
        if (alarm->trigger.type != ALARM_TRIG_ONCE) {
            // 循环闹钟，计算下次触发时间
            uint64_t next_ts = alarm_calc_next_trigger(alarm, (time_t)now_ts);
            if (next_ts == 0) {
                LISA_LOGE(TAG, "calc next trigger failed for expired alarm");
                
                return -1;
            }
            alarm->alarm_id = next_ts;
            LISA_LOGI(TAG, "expired alarm, updated to next trigger: %llu",
                     (unsigned long long)next_ts);
        } else {
            // 单次闹钟过期
            LISA_LOGW(TAG, "once alarm expired: %llu", (unsigned long long)alarm->alarm_id);
            // 可以选择返回错误或继续创建
        }
    }

    // 6. 保存循环信息
    if (params->day_of_week >= 1 && params->day_of_week <= 7) {
        alarm->trigger.day_of_week = (uint8_t)params->day_of_week;
    }
    if (params->day_of_month >= 1 && params->day_of_month <= 31) {
        alarm->trigger.day_of_month = (uint8_t)params->day_of_month;
    }
    if (params->month_of_year >= 1 && params->month_of_year <= 12) {
        alarm->trigger.month = (uint8_t)params->month_of_year;
    }
    if (params->holiday_name && params->holiday_name[0] != '\0') {
        strncpy(alarm->trigger.holiday_name, params->holiday_name,
                sizeof(alarm->trigger.holiday_name) - 1);
        alarm->trigger.holiday_name[sizeof(alarm->trigger.holiday_name) - 1] = '\0';
    }
    if (params->lunar_date_str && alarm->calendar == ALARM_CAL_LUNAR) {
        uint8_t m = 0, d = 0;
        if (alarm_time_parse_lunar(params->lunar_date_str, &m, &d) == 0) {
            alarm->trigger.lunar_month = m;
            alarm->trigger.lunar_day = d;
        }
    }

    // 7. 保存文本
    if (params->text && params->text[0] != '\0') {
        strncpy(alarm->text, params->text, sizeof(alarm->text) - 1);
        alarm->text[sizeof(alarm->text) - 1] = '\0';
    }

    // 8. 设置 Snooze 配置
    alarm->snooze_enabled = (params->snooze_enabled == -1) ? true : (params->snooze_enabled == 1);
    if(alarm->snooze_enabled){
        alarm->snooze_interval = (params->snooze_interval == -1) ? 5 : (uint8_t)params->snooze_interval;
        alarm->snooze_count = (params->snooze_count == -1) ? 3 : (uint8_t)params->snooze_count;
    }

    // 9. 打印闹钟创建信息
    int64_t current_ts = alarm_time_get_network_timestamp();
    if (current_ts > 0) {
        uint16_t cur_year;
        uint8_t cur_month, cur_day, cur_hour, cur_min, cur_sec;
        if (alarm_time_timestamp_to_date(current_ts, &cur_year, &cur_month, &cur_day,
                                &cur_hour, &cur_min, &cur_sec) == 0) {
            LISA_LOGI(TAG, "alarm created: current_network_time=%04d-%02d-%02d %02d:%02d:%02d, "
                        "trigger_time=%04d-%02d-%02d %02d:%02d:%02d, remaining_seconds=%llds",
                        cur_year, cur_month, cur_day, cur_hour, cur_min, cur_sec,
                        alarm->trigger.year, alarm->trigger.month, alarm->trigger.day,
                        alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second,
                        (long long)(alarm->alarm_id - current_ts));
        }
    }

    return 0;
}
