/**
 * @file model_alarm.h
 * @brief Alarm data model header
 */

#ifndef __MODEL_ALARM_H__
#define __MODEL_ALARM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef CONFIG_ALARM_TEXT_MAX_LEN
#define CONFIG_ALARM_TEXT_MAX_LEN 384
#endif

typedef struct {
    uint64_t timestamp;
    char text[CONFIG_ALARM_TEXT_MAX_LEN];
} alarm_data_t;

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int weekday;
    bool is_today;
    bool is_tomorrow;
} alarm_time_info_t;

void model_alarm_set_data(const alarm_data_t *data);
int model_alarm_get_data(alarm_data_t *data);
int model_alarm_get_time_info(uint64_t timestamp, alarm_time_info_t *info);
const char *model_alarm_get_event_text(void);
bool model_alarm_take_event_text(char *buf, size_t buf_size);
int model_alarm_init(void);
int model_alarm_delete_by_timestamp(uint64_t timestamp);
alarm_data_t *model_alarm_get_all(uint32_t *cnt);
void model_alarm_free(void *alarms);
uint32_t model_alarm_count_get(void);

/**
 * @brief 获取当前响铃闹钟的snooze剩余次数
 * @return -1 没有snooze或查询失败，>=0 表示剩余次数
 */
int model_alarm_get_snooze_remaining_count(void);

#ifdef __cplusplus
}
#endif

#endif /* __MODEL_ALARM_H__ */
