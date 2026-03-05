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

typedef struct {
    uint64_t timestamp;
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
int model_alarm_init(void);
int model_alarm_delete_by_timestamp(uint64_t timestamp);
alarm_data_t *model_alarm_get_all(uint32_t *cnt);
void model_alarm_free(void *alarms);
uint32_t model_alarm_count_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __MODEL_ALARM_H__ */
