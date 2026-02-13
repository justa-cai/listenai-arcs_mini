#ifndef __ALARM_STORE_H__
#define __ALARM_STORE_H__


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "alarm.h"
#include <stdbool.h>
typedef enum {
    ALARM_CAL_GREGORIAN = 0,
    ALARM_CAL_LUNAR = 1,
} alarm_calendar_t;

typedef enum {
    ALARM_TRIG_ONCE = 0,
    ALARM_TRIG_DAILY,
    ALARM_TRIG_WEEKLY,
    ALARM_TRIG_WORKDAY,
    ALARM_TRIG_WEEKEND,
    ALARM_TRIG_MONTHLY,
    ALARM_TRIG_YEARLY,
    ALARM_TRIG_HOLIDAY,
} alarm_trigger_type_t;

#define ALARM_KEYWORD_MAX 3
#define ALARM_KEYWORD_LEN 16

typedef struct {
    alarm_trigger_type_t type;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    uint16_t year;         // ONCE
    uint8_t month;         // ONCE/YEARLY
    uint8_t day;           // ONCE/YEARLY/MONTHLY
    uint8_t day_of_week;   // WEEKLY
    uint8_t day_of_month;  // MONTHLY
    uint8_t lunar_month;   // LUNAR (MM)
    uint8_t lunar_day;     // LUNAR (DD)
    char holiday_name[24]; // HOLIDAY
} alarm_trigger_t;

typedef struct {
    uint64_t alarm_id;      // timestamp as ID
    uint64_t cloud_id;      // cloud provided ID
    alarm_calendar_t calendar;
    alarm_trigger_t trigger;
    char text[LS_ALARM_TEXT_MAX_LEN];
    uint8_t keyword_count;
    char keywords[ALARM_KEYWORD_MAX][ALARM_KEYWORD_LEN];
} alarm_object_t;

typedef struct {
    uint32_t count;
    alarm_object_t items[LS_ALARM_MAX_COUNT];
} alarm_store_t;

typedef struct {
    alarm_object_t alarm;
    uint8_t keyword_count;
    char keywords[ALARM_KEYWORD_MAX][ALARM_KEYWORD_LEN];
    bool use_calendar;
    bool use_type;
    bool use_time;
    bool use_date;
    bool use_day_of_week;
    bool use_day_of_month;
    bool use_month_of_year;
    bool use_lunar_md;
    bool use_holiday_name;
    bool use_text;
} alarm_query_filter_t;

typedef struct {
    uint64_t cloud_id;
    uint8_t calendar;
    uint8_t trigger_type;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t day_of_week;
    uint8_t day_of_month;
    uint8_t lunar_month;
    uint8_t lunar_day;
    char holiday_name[24];
    uint16_t text_len;
    uint8_t keyword_count;
    char keywords[ALARM_KEYWORD_MAX][ALARM_KEYWORD_LEN];
} alarm_obj_nvs_t;

typedef struct {
    alarm_trigger_type_t type;
    const char *text_keyword;
    int day_of_week;
    int day_of_month;
    int month_of_year;
} alarm_query_t;

#define ALARM_LIST_KEY "user.alarm_list"

int alarm_store_init(void);
int alarm_store_create_obj(const alarm_object_t *alarm, char *err_msg, size_t err_len);
int alarm_store_create_obj_by_timestamp(const uint64_t timestamp, const uint8_t *text);
int alarm_store_update_obj(const alarm_object_t *alarm);
int alarm_store_update_obj_by_timestamp(const uint64_t timestamp, const uint8_t *text);
int alarm_store_delete_obj(const alarm_object_t *alarm);
int alarm_store_query_obj(const alarm_object_t *alarm, char *text_out, size_t buf_len);
void alarm_obj_print(const alarm_object_t *alarm);
uint64_t alarm_obj_to_timestamp(const alarm_object_t *alarm);
const alarm_store_t *alarm_store_get_all(void);
const alarm_object_t *alarm_store_find_by_id(uint64_t alarm_id);
const alarm_object_t *alarm_store_find_by_cloud_id(uint64_t cloud_id);
int alarm_store_query_list(const alarm_query_filter_t *filter,
                           const char *text_filter,
                           char *out_text,
                           size_t out_len);
int alarm_store_delete_by_filter(const alarm_query_filter_t *filter,
                                 const char *text_filter,
                                 char *out_text,
                                 size_t out_len);
int alarm_store_update_by_filter(const alarm_query_filter_t *filter,
                                 const alarm_object_t *update_data,
                                 char *out_text,
                                 size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
