#define TAG "alarm_time_utils"

#include "alarm_time_utils.h"
#include "lisa_log.h"
#include "listen_system.h"
#include "c_datetime.h"

#include <sys/time.h>
#include <string.h>

/* 北京时间偏移：UTC+8 */
#define CST_OFFSET_BY_UTC (8 * 3600)

int64_t alarm_time_get_network_timestamp(void)
{
    struct timeval tv;

    /* Prefer SNTP-synced calendar; fallback to gettimeofday */
    if (ls_sys_get_time(&tv) != 0) {
        if (gettimeofday(&tv, NULL) != 0) {
            LISA_LOGE(TAG, "failed to get network timestamp");
            return -1;
        }
    }

    return (int64_t)tv.tv_sec;
}

int alarm_time_timestamp_to_date(int64_t timestamp,
                            uint16_t *year, uint8_t *month, uint8_t *day,
                            uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if (timestamp < 0) {
        LISA_LOGE(TAG, "invalid timestamp: %lld", (long long)timestamp);
        return -1;
    }

    SHDateTime dt;
    /* timestampToDateObj 使用 UTC 时间，需要加上时区偏移得到北京时间 */
    if (timestampToDateObj((long)timestamp + CST_OFFSET_BY_UTC, 0, &dt) != 0) {
        LISA_LOGE(TAG, "timestampToDateObj failed for %lld", (long long)timestamp);
        return -1;
    }

    if (year) *year = (uint16_t)dt.year;
    if (month) *month = (uint8_t)dt.month;
    if (day) *day = (uint8_t)dt.day;
    if (hour) *hour = (uint8_t)dt.hour;
    if (minute) *minute = (uint8_t)dt.min;
    if (second) *second = (uint8_t)dt.sec;

    LISA_LOGD(TAG, "timestamp_to_date: %lld -> %04d-%02d-%02d %02d:%02d:%02d",
              (long long)timestamp, dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec);

    return 0;
}

uint64_t alarm_time_date_to_timestamp(uint16_t year, uint8_t month, uint8_t day,
                                 uint8_t hour, uint8_t minute, uint8_t second)
{
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59) {
        LISA_LOGE(TAG, "invalid datetime: %04d-%02d-%02d %02d:%02d:%02d",
                  year, month, day, hour, minute, second);
        return 0;
    }

    SHDateTime dt = {
        .year = year,
        .month = month,
        .day = day,
        .hour = hour,
        .min = minute,
        .sec = second,
        .timezoneOffset = 0
    };

    long timestamp = 0;
    if (dateObjToTimestamp(&dt, &timestamp) != 0) {
        LISA_LOGE(TAG, "dateObjToTimestamp failed for %04d-%02d-%02d %02d:%02d:%02d",
                  year, month, day, hour, minute, second);
        return 0;
    }

    /* 减去时区偏移，得到 UTC 时间戳 */
    timestamp -= CST_OFFSET_BY_UTC;

    LISA_LOGD(TAG, "date_to_timestamp: %04d-%02d-%02d %02d:%02d:%02d -> %ld",
              year, month, day, hour, minute, second, timestamp);

    return (uint64_t)timestamp;
}

int alarm_time_get_weekday(int64_t timestamp)
{
    if (timestamp < 0) {
        return -1;
    }

    int weekday = -1;
    /* calcWeekdayIdx 使用 UTC 时间，需要加上时区偏移 */
    if (calcWeekdayIdx((long)timestamp + CST_OFFSET_BY_UTC, 0, &weekday) != 0) {
        LISA_LOGE(TAG, "calcWeekdayIdx failed for %lld", (long long)timestamp);
        return -1;
    }

    return weekday;
}

/* ==================== 时间字符串解析 ==================== */

int alarm_time_parse_hms(const char *time_str, uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    int h = -1, m = -1, s = -1;

    if (!time_str || !hour || !minute || !second) {
        return -1;
    }

    if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
        // "HH:mm:ss" 格式
    } else if (sscanf(time_str, "%d:%d", &h, &m) == 2) {
        // "HH:mm" 格式，默认秒为 0
        s = 0;
    } else {
        LISA_LOGE(TAG, "invalid time format: %s", time_str);
        return -1;
    }

    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
        LISA_LOGE(TAG, "time out of range: %s", time_str);
        return -1;
    }

    *hour = (uint8_t)h;
    *minute = (uint8_t)m;
    *second = (uint8_t)s;

    LISA_LOGD(TAG, "parse_hms: %s -> %02d:%02d:%02d", time_str, h, m, s);
    return 0;
}

int alarm_time_parse_ymd(const char *date_str, uint16_t *year, uint8_t *month, uint8_t *day)
{
    int y = -1, m = -1, d = -1;

    if (!date_str || !year || !month || !day) {
        return -1;
    }

    if (sscanf(date_str, "%d-%d-%d", &y, &m, &d) != 3) {
        LISA_LOGE(TAG, "invalid date format: %s", date_str);
        return -1;
    }

    if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) {
        LISA_LOGE(TAG, "date out of range: %s", date_str);
        return -1;
    }

    *year = (uint16_t)y;
    *month = (uint8_t)m;
    *day = (uint8_t)d;

    LISA_LOGD(TAG, "parse_ymd: %s -> %04d-%02d-%02d", date_str, y, m, d);
    return 0;
}

int alarm_time_parse_lunar(const char *lunar_str, uint8_t *month, uint8_t *day)
{
    int m = -1, d = -1;

    if (!lunar_str || !month || !day) {
        return -1;
    }

    if (sscanf(lunar_str, "%d-%d", &m, &d) != 2) {
        LISA_LOGE(TAG, "invalid lunar format: %s", lunar_str);
        return -1;
    }

    if (m < 1 || m > 12 || d < 1 || d > 31) {
        LISA_LOGE(TAG, "lunar date out of range: %s", lunar_str);
        return -1;
    }

    *month = (uint8_t)m;
    *day = (uint8_t)d;

    LISA_LOGD(TAG, "parse_lunar: %s -> %02d-%02d", lunar_str, m, d);
    return 0;
}

/* ==================== 闹钟对象时间转换 ==================== */

uint64_t alarm_time_obj_to_timestamp(const alarm_object_t *alarm)
{
    if (!alarm) {
        return 0;
    }

    if (alarm->trigger.year < 1970 || alarm->trigger.month < 1 || alarm->trigger.day < 1) {
        LISA_LOGE(TAG, "invalid date for timestamp conversion: %u-%u-%u",
                  alarm->trigger.year, alarm->trigger.month, alarm->trigger.day);
        return 0;
    }

    uint64_t ts = alarm_time_date_to_timestamp(alarm->trigger.year, alarm->trigger.month, alarm->trigger.day,
                                     alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);
    if (ts == 0) {
        LISA_LOGE(TAG, "compose timestamp failed for %u-%u-%u %u:%u:%u",
                  alarm->trigger.year, alarm->trigger.month, alarm->trigger.day,
                  alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);
    }

    return ts;
}