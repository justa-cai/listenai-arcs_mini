#define TAG "alarm_control"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "listen_system.h"

#include "mcp.h"
#include "alarm_next.h"
#include "service_alarm.h"

#define ALARM_CTRL_FAIL(_msg)                            \
    do {                                                 \
        snprintf(result_text, text_len, "%s", (_msg));   \
        return -1;                                       \
    } while (0)

typedef struct {
    uint64_t cloud_id;             // cloud alarm id
    const char *action;            // CREATE / DELETE / UPDATE / QUERY
    const char *alarm_type;        // ONCE / DAILY / WEEKLY / WORKDAY / WEEKEND / MONTHLY / YEARLY
    const char *holiday_name;      // 节日名称
    const char *calendar_type;     // GREGORIAN / LUNAR
    const char *time_str;          // HH:mm:ss
    const char *date_str;          // YYYY-MM-DD
    const char *lunar_date_str;    // MM-DD (LUNAR)
    int day_of_week;               // 1-7
    int day_of_month;              // 1-31
    int month_of_year;             // 1-12
    const char *text;              // 提醒文本
    cJSON *search_keywords;        // array of strings
} alarm_control_params_t;


/* ==================== 参数解析工具 ==================== */

static void alarm_control_params_init(alarm_control_params_t *params)
{
    if (!params) {
        return;
    }
    memset(params, 0, sizeof(*params));
    params->cloud_id = 0;
    params->day_of_week = -1;
    params->day_of_month = -1;
    params->month_of_year = -1;
}

static void alarm_control_params_parse(cJSON *args, alarm_control_params_t *params)
{
    if (!args || !params) {
        return;
    }

    cJSON *pval = NULL;

    pval = mcp_tool_call_args_get(args, "id");
    if (pval && cJSON_IsNumber(pval)) {
        params->cloud_id = (uint64_t)pval->valuedouble;
    }

    pval = mcp_tool_call_args_get(args, "action");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->action = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "alarm_type");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->alarm_type = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "holiday_name");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->holiday_name = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "calendar_type");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->calendar_type = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "time");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->time_str = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "date");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->date_str = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "lunar_date");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->lunar_date_str = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "day_of_week");
    if (pval && cJSON_IsNumber(pval)) {
        params->day_of_week = pval->valueint;
    }

    pval = mcp_tool_call_args_get(args, "day_of_month");
    if (pval && cJSON_IsNumber(pval)) {
        params->day_of_month = pval->valueint;
    }

    pval = mcp_tool_call_args_get(args, "month_of_year");
    if (pval && cJSON_IsNumber(pval)) {
        params->month_of_year = pval->valueint;
    }

    pval = mcp_tool_call_args_get(args, "text");
    if (pval && cJSON_IsString(pval)) {
        params->text = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "search_keywords");
    if (pval && cJSON_IsArray(pval)) {
        params->search_keywords = pval;
    }
}


static alarm_calendar_t parse_calendar_type(const char *calendar_type)
{
    if (calendar_type && strcmp(calendar_type, "LUNAR") == 0) {
        return ALARM_CAL_LUNAR;
    }
    return ALARM_CAL_GREGORIAN;
}

static alarm_trigger_type_t parse_trigger_type(const char *alarm_type, const char *holiday_name)
{
    if (holiday_name && holiday_name[0] != '\0') {
        return ALARM_TRIG_HOLIDAY;
    }
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
    return ALARM_TRIG_ONCE;
}

static bool parse_time_hms(const char *time_str, uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    int h = -1, m = -1, s = -1;
    if (!time_str || !hour || !minute || !second) {
        return false;
    }

    if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
        // ok
    } else if (sscanf(time_str, "%d:%d", &h, &m) == 2) {
        s = 0;
    } else {
        return false;
    }

    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
        return false;
    }

    *hour = (uint8_t)h;
    *minute = (uint8_t)m;
    *second = (uint8_t)s;
    return true;
}

static bool parse_date_ymd(const char *date_str, uint16_t *year, uint8_t *month, uint8_t *day)
{
    int y = -1, m = -1, d = -1;
    if (!date_str || !year || !month || !day) {
        return false;
    }
    if (sscanf(date_str, "%d-%d-%d", &y, &m, &d) != 3) {
        return false;
    }
    if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) {
        return false;
    }
    *year = (uint16_t)y;
    *month = (uint8_t)m;
    *day = (uint8_t)d;
    return true;
}

static bool parse_lunar_md(const char *lunar_date_str, uint8_t *month, uint8_t *day)
{
    int m = -1, d = -1;
    if (!lunar_date_str || !month || !day) {
        return false;
    }
    if (sscanf(lunar_date_str, "%d-%d", &m, &d) != 2) {
        return false;
    }
    if (m < 1 || m > 12 || d < 1 || d > 31) {
        return false;
    }
    *month = (uint8_t)m;
    *day = (uint8_t)d;
    return true;
}

static void alarm_fill_keywords(const cJSON *arr, alarm_object_t *alarm)
{
    if (!arr || !alarm) {
        return;
    }

    uint8_t count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (!cJSON_IsString(item) || !item->valuestring) {
            continue;
        }
        if (count >= ALARM_KEYWORD_MAX) {
            break;
        }
        strncpy(alarm->keywords[count], item->valuestring, ALARM_KEYWORD_LEN - 1);
        alarm->keywords[count][ALARM_KEYWORD_LEN - 1] = '\0';
        count++;
    }
    alarm->keyword_count = count;
}

/* ==================== 闹钟日期处理工具 ==================== */

static void normalize_tm(struct tm *tmv);

static int get_current_tm(struct tm *out_tm)
{
    if (!out_tm) {
        return -1;
    }

    struct timeval tv;
    if (ls_sys_get_time(&tv) != 0) {
        if (gettimeofday(&tv, NULL) != 0) {
            return -1;
        }
    }

    long int ts = (long int)tv.tv_sec;
    if (!ls_sys_get_tmtime(&ts, out_tm)) {
        return -1;
    }

    // ls_sys_get_tmtime uses year base 1970 and does not set tm_wday.
    // Normalize via mktime with a 1900-based year offset, then convert back.
    normalize_tm(out_tm);

    return 0;
}

static void normalize_tm(struct tm *tmv)
{
    if (!tmv) {
        return;
    }

    struct tm tmp = *tmv;
    tmp.tm_year += 70; // 1970-based -> 1900-based
    mktime(&tmp);
    tmv->tm_year = tmp.tm_year - 70;
    tmv->tm_mon = tmp.tm_mon;
    tmv->tm_mday = tmp.tm_mday;
    tmv->tm_hour = tmp.tm_hour;
    tmv->tm_min = tmp.tm_min;
    tmv->tm_sec = tmp.tm_sec;
    tmv->tm_wday = tmp.tm_wday;
    tmv->tm_yday = tmp.tm_yday;
    tmv->tm_isdst = tmp.tm_isdst;
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

static void set_date_from_tm(alarm_object_t *alarm, const struct tm *tmv)
{
    if (!alarm || !tmv) {
        return;
    }
    alarm->trigger.year = (uint16_t)(tmv->tm_year + 1970);
    alarm->trigger.month = (uint8_t)(tmv->tm_mon + 1);
    alarm->trigger.day = (uint8_t)tmv->tm_mday;
}

static int calc_tm_seconds(const struct tm *tmv)
{
    if (!tmv) {
        return 0;
    }
    return tmv->tm_hour * 3600 + tmv->tm_min * 60 + tmv->tm_sec;
}

static int calc_alarm_seconds(const alarm_object_t *alarm)
{
    if (!alarm) {
        return 0;
    }
    return alarm->trigger.hour * 3600 +
           alarm->trigger.minute * 60 +
           alarm->trigger.second;
}

static int set_nearest_date_from_workday_rule(alarm_object_t *alarm, struct tm *tmv, bool want_workday)
{
    int now_sec = calc_tm_seconds(tmv);
    int alarm_sec = calc_alarm_seconds(alarm);

    LISA_LOGI(TAG, "workday calc start: want=%s now=%04d-%02d-%02d %02d:%02d:%02d alarm=%02u:%02u:%02u",
              want_workday ? "workday" : "weekend",
              tmv->tm_year + 1970, tmv->tm_mon + 1, tmv->tm_mday,
              tmv->tm_hour, tmv->tm_min, tmv->tm_sec,
              alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

    for (int i = 0; i < 370; i++) {
        // 查询日期是否工作日
        char date_str[16] = {0};
        snprintf(date_str, sizeof(date_str), "%02d-%02d", tmv->tm_mon + 1, tmv->tm_mday);
        bool is_work_day = false;
        if (listenai_date_is_workday(date_str, &is_work_day) != 0) {
            LISA_LOGE(TAG, "workday query failed, date=%s", date_str);
            return -6;
        }
        bool match = want_workday ? is_work_day : !is_work_day;
        
        LISA_LOGI(TAG, "workday calc: i=%d date=%04d-%02d-%02d is_workday=%d match=%d",
                  i,
                  tmv->tm_year + 1970, tmv->tm_mon + 1, tmv->tm_mday,
                  is_work_day, match);
  
        if (match) {
            if (i > 0 || alarm_sec > now_sec) {
                set_date_from_tm(alarm, tmv);
                return 0;
            }
        }
        tmv->tm_mday += 1;
        normalize_tm(tmv);
    }

    LISA_LOGW(TAG, "workday rule not found within limit");
    return -6;
}

// 获取未来最近的 周/月/年循环闹钟的日期
static int set_nearest_date_from_rule(alarm_object_t *alarm,
                                      alarm_trigger_type_t trigger_type,
                                      int day_of_week,
                                      int day_of_month,
                                      int month_of_year)
{
    struct tm tmv;
    if (get_current_tm(&tmv) != 0) {
        return -1;
    }

    // Ensure tm_wday is normalized
    normalize_tm(&tmv);

    int now_sec = calc_tm_seconds(&tmv);
    int alarm_sec = calc_alarm_seconds(alarm);

    

    LISA_LOGI(TAG, "set_nearest_date_from_rule: now=%02d:%02d:%02d alarm=%02u:%02u:%02u",
                tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
                alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);

    switch (trigger_type) {
    case ALARM_TRIG_DAILY:
    {
        if (alarm_sec <= now_sec) {
            tmv.tm_mday += 1;
            normalize_tm(&tmv);
        }
        set_date_from_tm(alarm, &tmv);
        return 0;
    }
    case ALARM_TRIG_WEEKLY:
    {
        if (day_of_week < 1 || day_of_week > 7) {
            return -2;
        }

        int today_wd = tmv.tm_wday == 0 ? 7 : tmv.tm_wday; // 1=Mon ... 7=Sun

        int diff = (day_of_week - today_wd + 7) % 7;
        if (diff == 0) {
            if (alarm_sec <= now_sec) {
                diff = 7;
            }
        }

        LISA_LOGI(TAG, "weekly calc: today_wd=%d target=%d diff=%d ",
                    today_wd, day_of_week, diff);

        tmv.tm_mday += diff;
        normalize_tm(&tmv);
        set_date_from_tm(alarm, &tmv);
        return 0;
    }
    case ALARM_TRIG_WORKDAY:
    {
        return set_nearest_date_from_workday_rule(alarm, &tmv, true);
    }
    case ALARM_TRIG_WEEKEND:
    {
        int today_wd = tmv.tm_wday == 0 ? 7 : tmv.tm_wday; // 1=Mon ... 7=Sun

        bool today_is_weekend = (today_wd == 6 || today_wd == 7);
        if (today_is_weekend && alarm_sec > now_sec) {
            set_date_from_tm(alarm, &tmv);
            return 0;
        }

        tmv.tm_mday += 1;
        normalize_tm(&tmv);
        for (int i = 0; i < 7; i++) {
            int wd = tmv.tm_wday == 0 ? 7 : tmv.tm_wday;
            if (wd == 6 || wd == 7) {
                set_date_from_tm(alarm, &tmv);
                return 0;
            }
            tmv.tm_mday += 1;
            normalize_tm(&tmv);
        }

        LISA_LOGW(TAG, "weekend rule not found within limit");
        return -6;
    }
    case ALARM_TRIG_MONTHLY:
    {
        if (day_of_month < 1 || day_of_month > 31) {
            return -3;
        }
        LISA_LOGI(TAG, "monthly calc: today=%04d-%02d-%02d now=%02d:%02d:%02d target_day=%d ",
                    tmv.tm_year + 1970, tmv.tm_mon + 1, tmv.tm_mday,
                    tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

        int max_day = days_in_month(tmv.tm_year + 1970, tmv.tm_mon + 1);
        int target_day = day_of_month > max_day ? max_day : day_of_month;
        if (tmv.tm_mday < target_day) {
            tmv.tm_mday = target_day;
        } else if (tmv.tm_mday == target_day) {
            if (alarm_sec <= now_sec) {
                tmv.tm_mon += 1;
            }
            max_day = days_in_month(tmv.tm_year + 1970, tmv.tm_mon + 1);
            target_day = day_of_month > max_day ? max_day : day_of_month;
            tmv.tm_mday = target_day;
        } else {
            tmv.tm_mon += 1;
            max_day = days_in_month(tmv.tm_year + 1970, tmv.tm_mon + 1);
            target_day = day_of_month > max_day ? max_day : day_of_month;
            tmv.tm_mday = target_day;
        }
    
        normalize_tm(&tmv);
        set_date_from_tm(alarm, &tmv);
        return 0;
    }
    case ALARM_TRIG_YEARLY:
    {
        if (month_of_year < 1 || month_of_year > 12 || day_of_month < 1 || day_of_month > 31) {
            return -4;
        }
        LISA_LOGI(TAG, "yearly calc: today=%04d-%02d-%02d now=%02d:%02d:%02d target=%02d-%02d ",
                    tmv.tm_year + 1970, tmv.tm_mon + 1, tmv.tm_mday,
                    tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
                    month_of_year, day_of_month);

        int max_day = days_in_month(tmv.tm_year + 1970, month_of_year);
        int target_day = day_of_month > max_day ? max_day : day_of_month;
        if ((tmv.tm_mon + 1 < month_of_year) ||
            ((tmv.tm_mon + 1 == month_of_year) && (tmv.tm_mday < target_day))) {
            tmv.tm_mon = month_of_year - 1;
            tmv.tm_mday = target_day;

        } else if ((tmv.tm_mon + 1 == month_of_year) && (tmv.tm_mday == target_day)) {
            if (alarm_sec <= now_sec) {
                tmv.tm_year += 1;
            }
            tmv.tm_mon = month_of_year - 1;
            max_day = days_in_month(tmv.tm_year + 1970, month_of_year);
            target_day = day_of_month > max_day ? max_day : day_of_month;
            tmv.tm_mday = target_day;
        } else {
            tmv.tm_year += 1;
            tmv.tm_mon = month_of_year - 1;
            max_day = days_in_month(tmv.tm_year + 1970, month_of_year);
            target_day = day_of_month > max_day ? max_day : day_of_month;
            tmv.tm_mday = target_day;
        }
    
        normalize_tm(&tmv);
        set_date_from_tm(alarm, &tmv);
        return 0;
    }
    default:
        return -5;
    }
}

/* ==================== 闹钟对象构造工具 ==================== */
static int build_alarm_by_params(const alarm_control_params_t *params, alarm_object_t *alarm_obj,
                                  char *err_msg, size_t err_len)
{
    if (!params || !alarm_obj || !err_msg || err_len == 0) {
        return -1;
    }

    memset(alarm_obj, 0, sizeof(*alarm_obj));
    alarm_obj->alarm_id = 0;
    alarm_obj->cloud_id = params->cloud_id; 
    alarm_obj->calendar = parse_calendar_type(params->calendar_type);
    alarm_obj->trigger.type = parse_trigger_type(params->alarm_type, params->holiday_name);

    // 检查闹钟时间（必选）
    if (!params->time_str || params->time_str[0] == '\0') {
        LISA_LOGE(TAG, "time missing");
        snprintf(err_msg, err_len, "没有指定时间或时间格式错误");
        return -1;
    }
    if (!parse_time_hms(params->time_str, &alarm_obj->trigger.hour, &alarm_obj->trigger.minute, &alarm_obj->trigger.second)) {
        LISA_LOGE(TAG, "invalid time format: %s", params->time_str);
        snprintf(err_msg, err_len, "没有指定时间或时间格式错误");
        return -1;
    }

    // 检查闹钟日期
    bool date_set = false;
    if (params->date_str) {
        uint16_t y = 0;
        uint8_t m = 0;
        uint8_t d = 0;
        if (parse_date_ymd(params->date_str, &y, &m, &d)) {
            alarm_obj->trigger.year = y;
            alarm_obj->trigger.month = m;
            alarm_obj->trigger.day = d;
            date_set = true;
        }
    }
    // 如果没有下发日期，构造闹钟日期
    // 先按日历类型构造
    if (!date_set) {
        if (alarm_obj->calendar == ALARM_CAL_LUNAR) {
            if (!params->lunar_date_str || params->lunar_date_str[0] == '\0') {
                LISA_LOGE(TAG, "lunar_date missing");
                snprintf(err_msg, err_len, "缺少农历日期");
                return -1;
            }
            char gregorian_date[16] = {0};
            if (listenai_date_transition("lunar", params->lunar_date_str,
                                         gregorian_date, sizeof(gregorian_date)) != 0) {
                LISA_LOGE(TAG, "lunar transition failed");
                snprintf(err_msg, err_len, "农历日期转换失败");
                return -1;
            }
            uint16_t y = 0;
            uint8_t m = 0;
            uint8_t d = 0;
            if (!parse_date_ymd(gregorian_date, &y, &m, &d)) {
                LISA_LOGE(TAG, "invalid lunar transition date: %s", gregorian_date);
                snprintf(err_msg, err_len, "农历日期转换失败");
                return -1;
            }
            alarm_obj->trigger.year = y;
            alarm_obj->trigger.month = m;
            alarm_obj->trigger.day = d;
            date_set = true;
        }
    }
    // 再按闹钟类型构造
    if (!date_set) {
        if (alarm_obj->trigger.type == ALARM_TRIG_ONCE) {
            struct tm tmv;
            if (get_current_tm(&tmv) != 0) {
                LISA_LOGE(TAG, "get today date failed");
                snprintf(err_msg, err_len, "无法获取当前日期");
                return -1;
            }
            set_date_from_tm(alarm_obj, &tmv);
            date_set = true;
        } else if (alarm_obj->trigger.type == ALARM_TRIG_DAILY  ||
                   alarm_obj->trigger.type == ALARM_TRIG_WEEKLY ||
                   alarm_obj->trigger.type == ALARM_TRIG_MONTHLY ||
                   alarm_obj->trigger.type == ALARM_TRIG_YEARLY ||
                   alarm_obj->trigger.type == ALARM_TRIG_WORKDAY||
                   alarm_obj->trigger.type == ALARM_TRIG_WEEKEND ) {
            int r = set_nearest_date_from_rule(alarm_obj, alarm_obj->trigger.type,
                                               params->day_of_week, params->day_of_month, params->month_of_year);
            if (r != 0) {
                LISA_LOGE(TAG, "missing or invalid rule params, type=%d", alarm_obj->trigger.type);
                snprintf(err_msg, err_len, "缺少必要的日期参数");
                return -1;
            }
        }else if (alarm_obj->trigger.type == ALARM_TRIG_HOLIDAY) {
            if (!params->holiday_name || params->holiday_name[0] == '\0') {
                LISA_LOGE(TAG, "holiday_name missing");
                snprintf(err_msg, err_len, "缺少节日名称");
                return -1;
            }
            char gregorian_date[16] = {0};
            if (listenai_date_transition("holiday", params->holiday_name,
                                         gregorian_date, sizeof(gregorian_date)) != 0) {
                LISA_LOGE(TAG, "holiday transition failed");
                snprintf(err_msg, err_len, "节假日转换失败");
                return -1;
            }
            uint16_t y = 0;
            uint8_t m = 0;
            uint8_t d = 0;
            if (!parse_date_ymd(gregorian_date, &y, &m, &d)) {
                LISA_LOGE(TAG, "invalid holiday transition date: %s", gregorian_date);
                snprintf(err_msg, err_len, "节假日转换失败");
                return -1;
            }
            alarm_obj->trigger.year = y;
            alarm_obj->trigger.month = m;
            alarm_obj->trigger.day = d;
            date_set = true;
        }
    }

    // 保存时间戳
    alarm_obj->alarm_id = alarm_obj_to_timestamp(alarm_obj);
    if (alarm_obj->alarm_id == 0) {
        LISA_LOGE(TAG, "invalid alarm datetime, cannot build alarm_id");
        snprintf(err_msg, err_len, "闹钟时间无效");
        return -1;
    }

    // 保存循环信息
    if (params->day_of_week >= 1 && params->day_of_week <= 7) {
        alarm_obj->trigger.day_of_week = (uint8_t)params->day_of_week;
    }
    if (params->day_of_month >= 1 && params->day_of_month <= 31) {
        alarm_obj->trigger.day_of_month = (uint8_t)params->day_of_month;
    }
    if (params->month_of_year >= 1 && params->month_of_year <= 12) {
        alarm_obj->trigger.month = (uint8_t)params->month_of_year;
    }
    if (params->holiday_name && params->holiday_name[0] != '\0') {
        strncpy(alarm_obj->trigger.holiday_name, params->holiday_name, sizeof(alarm_obj->trigger.holiday_name) - 1);
        alarm_obj->trigger.holiday_name[sizeof(alarm_obj->trigger.holiday_name) - 1] = '\0';
    }
    if (params->lunar_date_str && alarm_obj->calendar == ALARM_CAL_LUNAR) {
        uint8_t m = 0;
        uint8_t d = 0;
        if (parse_lunar_md(params->lunar_date_str, &m, &d)) {
            alarm_obj->trigger.lunar_month = m;
            alarm_obj->trigger.lunar_day = d;
        } else {
            LISA_LOGW(TAG, "invalid lunar_date format: %s", params->lunar_date_str);
        }
    }

    // 保存文本和关键词
    if (params->text && params->text[0] != '\0') {
        strncpy(alarm_obj->text, params->text, sizeof(alarm_obj->text) - 1);
        alarm_obj->text[sizeof(alarm_obj->text) - 1] = '\0';
    }
    if (params->search_keywords) {
        alarm_fill_keywords(params->search_keywords, alarm_obj);
    }

    return 0;
}

/* ==================== 闹钟对象查找工具 ==================== */
static int delete_alarm_by_cloud_id(uint64_t cloud_id, char *result_text, size_t text_len)
{
    return service_alarm_delete_by_cloud_id(cloud_id, result_text, text_len);
}


/**
 * @brief 闹钟提醒相关功能的处理函数
 *
 * 该工具用于处理闹钟提醒的创建、查询、删除操作。
 * 首先构造 alarm_obj 闹钟对象，
 * 然后基于 action 调用不同的函数处理闹钟操作指令，
 * 最后基于处理结果构造 MCP 响应。
 */
static int alarm_control_execute(const alarm_control_params_t *params, char *result_text, size_t text_len)
{
    if (!params || !result_text || text_len == 0) {
        ALARM_CTRL_FAIL("缺少参数");
    }
    if (!params->action || params->action[0] == '\0') {
        ALARM_CTRL_FAIL("缺少操作类型");
    }
    if (params->cloud_id == 0) {
        ALARM_CTRL_FAIL("缺少闹钟id");
    }

    result_text[0] = '\0';

    // 执行闹钟指令
    if (params->action && strcmp(params->action, "CREATE") == 0) {
        alarm_object_t alarm_obj;
        memset(&alarm_obj, 0, sizeof(alarm_obj));
        if (build_alarm_by_params(params, &alarm_obj, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
        alarm_obj_print(&alarm_obj);

        if (service_alarm_create_obj(&alarm_obj, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
    } else if (params->action && strcmp(params->action, "DELETE") == 0) {
        if (service_alarm_delete_by_cloud_id(params->cloud_id, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
    } else if (params->action && strcmp(params->action, "UPDATE") == 0) {
        if (service_alarm_delete_by_cloud_id(params->cloud_id, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }

        alarm_object_t new_alarm;
        memset(&new_alarm, 0, sizeof(new_alarm));
        if (build_alarm_by_params(params, &new_alarm, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
        alarm_obj_print(&new_alarm);

        if (service_alarm_create_obj(&new_alarm, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
    } else if (params->action && strcmp(params->action, "QUERY") == 0) {
        const alarm_object_t *target = service_alarm_find_by_cloud_id(params->cloud_id);
        if (!target) {
            ALARM_CTRL_FAIL("未找到对应闹钟");
        }

        snprintf(result_text, text_len, "查询成功, id=%llu, timestamp=%llu, text=%s",
                 (unsigned long long)target->cloud_id,
                 (unsigned long long)target->alarm_id,
                 target->text);
    } else {
        ALARM_CTRL_FAIL("不支持的操作类型");
    }
    
    return 0;
}



static cJSON *alarm_control_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    alarm_control_params_t params;
    alarm_control_params_init(&params);
    alarm_control_params_parse(args, &params);

    char result_text[512] = {0};
    int exec_ret = alarm_control_execute(&params, result_text, sizeof(result_text));

    if (result_text[0] == '\0') {
        snprintf(result_text, sizeof(result_text), "%s", (exec_ret == 0) ? "闹钟操作成功" : "闹钟操作失败");
    }

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "content", result_text);
    cJSON_AddBoolToObject(result, "isError", exec_ret != 0);

    return result;
}


static cJSON *alarm_control_list(const char *name)
{
    cJSON *tool = cJSON_CreateObject();
    if (!tool) {
        return NULL;
    }

    cJSON_AddStringToObject(tool, "name", name);
    cJSON_AddStringToObject(tool, "description",
                            "智能闹钟与提醒管理系统。支持公历/农历、固定周期及动态节假日。核心准则：1.【禁止猜测时间】若用户仅提供‘下午’、‘早晨’、‘待会’、‘以后’等模糊时段，或未提及具体时间，严禁私自编造时间点，此时必须放弃调用函数，转而向用户询问具体几点几分。2. 识别到特定节日必须填充 holiday_name；3. 只要存在提醒内容(text)，就必须同步提取搜索关键词(search_keywords)以供后续索引。");

    cJSON *schema = cJSON_CreateObject();
    if (!schema) {
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON_AddStringToObject(schema, "type", "object");

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        cJSON_Delete(schema);
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON *action_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(action_prop, "type", "string");
    cJSON *action_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("CREATE"));
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("DELETE"));
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("UPDATE"));
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("QUERY"));
    cJSON_AddItemToObject(action_prop, "enum", action_enum);
    cJSON_AddStringToObject(action_prop, "description", "操作类型：CREATE(新建), DELETE(删除), UPDATE(修改), QUERY(查询)。");
    cJSON_AddItemToObject(properties, "action", action_prop);

    cJSON *alarm_type_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(alarm_type_prop, "type", "string");
    cJSON *alarm_type_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("ONCE"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("DAILY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("WEEKLY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("WORKDAY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("WEEKEND"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("MONTHLY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("YEARLY"));
    cJSON_AddItemToObject(alarm_type_prop, "enum", alarm_type_enum);
    cJSON_AddStringToObject(alarm_type_prop, "description", "循环策略。节日提醒固定使用 YEARLY。");
    cJSON_AddItemToObject(properties, "alarm_type", alarm_type_prop);

    cJSON *holiday_name_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(holiday_name_prop, "type", "string");
    cJSON_AddStringToObject(holiday_name_prop, "description", "节日名称（如'中秋节'、'母亲节'）。若用户提及节日，此项必填。");
    cJSON_AddItemToObject(properties, "holiday_name", holiday_name_prop);

    cJSON *calendar_type_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(calendar_type_prop, "type", "string");
    cJSON *calendar_type_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(calendar_type_enum, cJSON_CreateString("GREGORIAN"));
    cJSON_AddItemToArray(calendar_type_enum, cJSON_CreateString("LUNAR"));
    cJSON_AddItemToObject(calendar_type_prop, "enum", calendar_type_enum);
    cJSON_AddStringToObject(calendar_type_prop, "default", "GREGORIAN");
    cJSON_AddStringToObject(calendar_type_prop, "description", "历法：GREGORIAN(公历), LUNAR(农历)。");
    cJSON_AddItemToObject(properties, "calendar_type", calendar_type_prop);

    cJSON *time_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(time_prop, "type", "string");
    cJSON_AddStringToObject(time_prop, "description", "24小时制时间 (HH:mm:ss)。严禁推测，未提及则反问用户。");
    cJSON_AddItemToObject(properties, "time", time_prop);

    cJSON *date_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(date_prop, "type", "string");
    cJSON_AddStringToObject(date_prop, "description", "具体日期 (YYYY-MM-DD)。仅在 ONCE 模式下使用。");
    cJSON_AddItemToObject(properties, "date", date_prop);

    cJSON *lunar_date_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(lunar_date_prop, "type", "string");
    cJSON_AddStringToObject(lunar_date_prop, "description",
                            "具体日期 (MM-DD)。仅在 calendar_type 为 LUNAR 模式下使用。需要输出农历对应的数字月日格式，如‘正月初三’输出‘01-03’。");
    cJSON_AddItemToObject(properties, "lunar_date", lunar_date_prop);

    cJSON *day_of_week_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(day_of_week_prop, "type", "integer");
    cJSON_AddNumberToObject(day_of_week_prop, "minimum", 1);
    cJSON_AddNumberToObject(day_of_week_prop, "maximum", 7);
    cJSON_AddStringToObject(day_of_week_prop, "description", "周几（1-7，1代表周一）。仅在 WEEKLY 模式有效。");
    cJSON_AddItemToObject(properties, "day_of_week", day_of_week_prop);

    cJSON *day_of_month_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(day_of_month_prop, "type", "integer");
    cJSON_AddNumberToObject(day_of_month_prop, "minimum", 1);
    cJSON_AddNumberToObject(day_of_month_prop, "maximum", 31);
    cJSON_AddStringToObject(day_of_month_prop, "description", "每月几号。仅在 MONTHLY 或无节日的 YEARLY 模式有效。");
    cJSON_AddItemToObject(properties, "day_of_month", day_of_month_prop);

    cJSON *month_of_year_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(month_of_year_prop, "type", "integer");
    cJSON_AddNumberToObject(month_of_year_prop, "minimum", 1);
    cJSON_AddNumberToObject(month_of_year_prop, "maximum", 12);
    cJSON_AddStringToObject(month_of_year_prop, "description", "每年几月。仅在无节日的 YEARLY 模式有效。");
    cJSON_AddItemToObject(properties, "month_of_year", month_of_year_prop);

    cJSON *text_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(text_prop, "type", "string");
    cJSON_AddStringToObject(text_prop, "description", "提醒的完整文本内容。例如：'给妈妈打电话庆祝生日'。");
    cJSON_AddItemToObject(properties, "text", text_prop);

    cJSON *search_keywords_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(search_keywords_prop, "type", "array");
    cJSON *keywords_items = cJSON_CreateObject();
    cJSON_AddStringToObject(keywords_items, "type", "string");
    cJSON_AddItemToObject(search_keywords_prop, "items", keywords_items);
    cJSON_AddStringToObject(search_keywords_prop, "description", "【强制要求】从 text 中提取的 1-3 个核心动词或名词。若 text 存在，此项禁止为空。例如 text 为'记得下午三点喝水'，关键词为 ['喝水']。");
    cJSON_AddItemToObject(properties, "search_keywords", search_keywords_prop);

    cJSON_AddItemToObject(schema, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("action"));
    cJSON_AddItemToObject(schema, "required", required);

    cJSON_AddItemToObject(tool, "inputSchema", schema);

    return tool;
}

MCP_TOOL_DEFINE(ls.built_in.alarm_clock, alarm_control_list, alarm_control_call);
