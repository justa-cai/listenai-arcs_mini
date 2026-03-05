#define TAG "alarm_store"

#include "alarm_store.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "lisa_log.h"
#include "lisa_kv.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "sysutils.h"

#include "alarm.h"
#include "alarm_next.h"

static alarm_store_t s_alarm_store __psram_bss__;
static lisa_mutex_t *s_alarm_store_mutex = NULL;

/* ==================== 工具函数 ==================== */

static void alarm_store_cache_add(const alarm_object_t *alarm)
{
    if (!alarm) {
        return;
    }

    lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
    for (uint32_t i = 0; i < s_alarm_store.count; ++i) {
        if (s_alarm_store.items[i].alarm_id == alarm->alarm_id) {
            s_alarm_store.items[i] = *alarm;
            lisa_mutex_unlock(s_alarm_store_mutex);
            return;
        }
    }

    if (s_alarm_store.count < LS_ALARM_MAX_COUNT) {
        s_alarm_store.items[s_alarm_store.count++] = *alarm;
    } else {
        LISA_LOGW(TAG, "alarm store full, skip cache add id=%llu",
                  (unsigned long long)alarm->alarm_id);
    }
    lisa_mutex_unlock(s_alarm_store_mutex);
}

static void alarm_store_cache_remove(uint64_t alarm_id)
{
    lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
    for (uint32_t i = 0; i < s_alarm_store.count; ++i) {
        if (s_alarm_store.items[i].alarm_id == alarm_id) {
            for (uint32_t j = i + 1; j < s_alarm_store.count; ++j) {
                s_alarm_store.items[j - 1] = s_alarm_store.items[j];
            }
            s_alarm_store.count--;
            break;
        }
    }
    lisa_mutex_unlock(s_alarm_store_mutex);
}

static int get_network_time(struct tm *network_time, time_t *network_timestamp)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        return -1;
    }

    if (network_timestamp) {
        *network_timestamp = (time_t)tv.tv_sec;
    }
    if (network_time) {
        localtime_r(&tv.tv_sec, network_time);
    }

    return 0;
}

static const char *alarm_calendar_name(alarm_calendar_t calendar)
{
    switch (calendar) {
    case ALARM_CAL_GREGORIAN:
        return "GREGORIAN";
    case ALARM_CAL_LUNAR:
        return "LUNAR";
    default:
        return "UNKNOWN";
    }
}

static const char *alarm_trigger_type_name(alarm_trigger_type_t type)
{
    switch (type) {
    case ALARM_TRIG_ONCE:
        return "ONCE";
    case ALARM_TRIG_DAILY:
        return "DAILY";
    case ALARM_TRIG_WEEKLY:
        return "WEEKLY";
    case ALARM_TRIG_WORKDAY:
        return "WORKDAY";
    case ALARM_TRIG_WEEKEND:
        return "WEEKEND";
    case ALARM_TRIG_MONTHLY:
        return "MONTHLY";
    case ALARM_TRIG_YEARLY:
        return "YEARLY";
    case ALARM_TRIG_HOLIDAY:
        return "HOLIDAY";
    default:
        return "UNKNOWN";
    }
}

void alarm_obj_print(const alarm_object_t *alarm)
{
    if (!alarm) {
        LISA_LOGW(TAG, "alarm_obj_log: null alarm");
        return;
    }

    char keywords_buf[ALARM_KEYWORD_MAX * ALARM_KEYWORD_LEN] = {0};
    size_t off = 0;
    for (uint8_t i = 0; i < alarm->keyword_count && i < ALARM_KEYWORD_MAX; i++) {
        if (alarm->keywords[i][0] == '\0') {
            continue;
        }
        int n = snprintf(keywords_buf + off, sizeof(keywords_buf) - off,
                         "%s%s", (off > 0) ? "," : "", alarm->keywords[i]);
        if (n < 0 || (size_t)n >= sizeof(keywords_buf) - off) {
            break;
        }
        off += (size_t)n;
    }

    char buf[512];
    size_t used = 0;

    // helper macro to append with bounds check
#define APPEND_FIELD(_fmt, ...)                                     \
    do {                                                            \
        if (used < sizeof(buf)) {                                   \
            int _n = snprintf(buf + used, sizeof(buf) - used, _fmt, __VA_ARGS__); \
            if (_n > 0) {                                           \
                size_t _added = (size_t)_n;                         \
                used += (_added < (sizeof(buf) - used)) ? _added : (sizeof(buf) - used - 1); \
            }                                                       \
        }                                                           \
    } while (0)

    APPEND_FIELD("%s", "alarm_obj:");

    if (alarm->cloud_id != 0) {
        APPEND_FIELD(" cloud_id=%llu", (unsigned long long)alarm->cloud_id);
    }
    if (alarm->alarm_id != 0) {
        APPEND_FIELD(" id=%llu", (unsigned long long)alarm->alarm_id);
    }
    APPEND_FIELD(" cal=%s", alarm_calendar_name(alarm->calendar));
    APPEND_FIELD(" type=%s", alarm_trigger_type_name(alarm->trigger.type));

    if (alarm->trigger.hour || alarm->trigger.minute || alarm->trigger.second) {
        APPEND_FIELD(" time=%02u:%02u:%02u", (unsigned)alarm->trigger.hour, (unsigned)alarm->trigger.minute, (unsigned)alarm->trigger.second);
    }
    if (alarm->trigger.year || alarm->trigger.month || alarm->trigger.day) {
        APPEND_FIELD(" date=%04u-%02u-%02u", (unsigned)alarm->trigger.year, (unsigned)alarm->trigger.month, (unsigned)alarm->trigger.day);
    }
    if (alarm->trigger.day_of_week) {
        APPEND_FIELD(" day_of_week=%u", (unsigned)alarm->trigger.day_of_week);
    }
    if (alarm->trigger.day_of_month) {
        APPEND_FIELD(" day_of_month=%u", (unsigned)alarm->trigger.day_of_month);
    }
    if (alarm->trigger.lunar_month || alarm->trigger.lunar_day) {
        APPEND_FIELD(" lunar=%02u-%02u", (unsigned)alarm->trigger.lunar_month, (unsigned)alarm->trigger.lunar_day);
    }
    if (alarm->trigger.holiday_name[0]) {
        APPEND_FIELD(" holiday=%s", alarm->trigger.holiday_name);
    }
    if (alarm->text[0]) {
        APPEND_FIELD(" text=%s", (const char *)alarm->text);
    }
    if (keywords_buf[0]) {
        APPEND_FIELD(" keywords=%s", keywords_buf);
    }

    shellPrint(shellGetCurrent(), "%s", buf);

#undef APPEND_FIELD
}


#define BEIJING_TIME_OFFSET_SEC (8 * 60 * 60)
uint64_t alarm_obj_to_timestamp(const alarm_object_t *alarm)
{
    if (!alarm) {
        return 0;
    }


    if (alarm->trigger.year < 1970 || alarm->trigger.month < 1 || alarm->trigger.day < 1) {
        LISA_LOGE(TAG, "invalid date for timestamp conversion: %u-%u-%u",
                  alarm->trigger.year, alarm->trigger.month, alarm->trigger.day);
        return 0;
    }

    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = (int)alarm->trigger.year - 1900;
    tmv.tm_mon = (int)alarm->trigger.month - 1;
    tmv.tm_mday = (int)alarm->trigger.day;
    tmv.tm_hour = (int)alarm->trigger.hour;
    tmv.tm_min = (int)alarm->trigger.minute;
    tmv.tm_sec = (int)alarm->trigger.second;

    time_t ts = mktime(&tmv);
    if (ts < 0) {
        LISA_LOGE(TAG, "mktime failed for %u-%u-%u %u:%u:%u",
                  alarm->trigger.year, alarm->trigger.month, alarm->trigger.day,
                  alarm->trigger.hour, alarm->trigger.minute, alarm->trigger.second);
        return 0;
    }

    ts -= BEIJING_TIME_OFFSET_SEC;

    return (uint64_t)ts;
}

static int alarm_obj_from_timestamp(uint64_t timestamp, const uint8_t *text, alarm_object_t *out_alarm)
{
    if (!out_alarm || timestamp == 0) {
        return -1;
    }

    memset(out_alarm, 0, sizeof(*out_alarm));
    out_alarm->alarm_id = timestamp;
    out_alarm->calendar = ALARM_CAL_GREGORIAN;
    out_alarm->trigger.type = ALARM_TRIG_ONCE;

    time_t ts = (time_t)timestamp;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    if (!localtime_r(&ts, &tmv)) {
        return -1;
    }

    out_alarm->trigger.year = (uint16_t)(tmv.tm_year + 1970);
    out_alarm->trigger.month = (uint8_t)(tmv.tm_mon + 1);
    out_alarm->trigger.day = (uint8_t)tmv.tm_mday;
    out_alarm->trigger.hour = (uint8_t)tmv.tm_hour;
    out_alarm->trigger.minute = (uint8_t)tmv.tm_min;
    out_alarm->trigger.second = (uint8_t)tmv.tm_sec;

    if (text) {
        size_t copy_len = strnlen((const char *)text, sizeof(out_alarm->text) - 1);
        memcpy(out_alarm->text, text, copy_len);
        out_alarm->text[copy_len] = '\0';
    }

    return 0;
}

/* ==================== Alarm_list NVS 管理工具 ==================== */

static int alarm_list_load(uint64_t **ids, uint32_t *count)
{
    int len = 0;
    uint8_t *data = NULL;

    if (ids) {
        *ids = NULL;
    }
    if (count) {
        *count = 0;
    }

    int ret = lisa_kv_get_blob(ALARM_LIST_KEY, &data, &len);
    if (ret != 0 || !data || len <= 0) {
        if (data) {
            lisa_mem_free(data);
        }
        return 0;
    }

    if (len % (int)sizeof(uint64_t) != 0) {
        lisa_mem_free(data);
        return -1;
    }

    if (ids) {
        *ids = (uint64_t *)data;
    } else {
        lisa_mem_free(data);
        return -1;
    }

    if (count) {
        *count = (uint32_t)(len / sizeof(uint64_t));
    }

    return 0;
}

static bool alarm_list_contains(const uint64_t *ids, uint32_t count, uint64_t alarm_id)
{
    for (uint32_t i = 0; i < count; i++) {
        if (ids[i] == alarm_id) {
            return true;
        }
    }
    return false;
}

static int alarm_list_save(const uint64_t *ids, uint32_t count)
{
    if (!ids || count == 0) {
        return lisa_kv_set_blob(ALARM_LIST_KEY, (uint8_t *)ids, 0);
    }
    return lisa_kv_set_blob(ALARM_LIST_KEY, (uint8_t *)ids, count * sizeof(uint64_t));
}

// 添加一个id到闹钟ID列表
static int alarm_list_add(uint64_t id)
{
    uint64_t *ids = NULL;
    uint32_t count = 0;
    int ret = alarm_list_load(&ids, &count);
    if (ret != 0) {
        if (ids) lisa_mem_free(ids);
        return -1;
    }
    // 检查是否已存在
    for (uint32_t i = 0; i < count; ++i) {
        if (ids[i] == id) {
            if (ids) lisa_mem_free(ids);
            return 0; // 已存在无需添加
        }
    }
    uint64_t *new_ids = lisa_mem_alloc((count + 1) * sizeof(uint64_t));
    if (!new_ids) {
        if (ids) lisa_mem_free(ids);
        return -1;
    }
    if (count > 0 && ids) {
        memcpy(new_ids, ids, count * sizeof(uint64_t));
    }
    new_ids[count] = id;
    ret = alarm_list_save(new_ids, count + 1);
    lisa_mem_free(new_ids);
    if (ids) lisa_mem_free(ids);
    return ret;
}

// 从闹钟ID列表中删除一个id
static int alarm_list_remove(uint64_t id)
{
    uint64_t *ids = NULL;
    uint32_t count = 0;
    int ret = alarm_list_load(&ids, &count);
    if (ret != 0 || count == 0) {
        if (ids) lisa_mem_free(ids);
        return -1;
    }
    uint32_t new_count = 0;
    uint64_t *new_ids = lisa_mem_alloc(count * sizeof(uint64_t));
    if (!new_ids) {
        if (ids) lisa_mem_free(ids);
        return -1;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (ids[i] != id) {
            new_ids[new_count++] = ids[i];
        }
    }
    if (new_count == 0) {
        ret = lisa_kv_del(ALARM_LIST_KEY);
    } else {
        ret = lisa_kv_set_blob(ALARM_LIST_KEY, (uint8_t *)new_ids, new_count * sizeof(uint64_t));
    }
    lisa_mem_free(new_ids);
    if (ids) lisa_mem_free(ids);
    return ret;
}

/* ==================== Alarm_obj NVS 管理工具 ==================== */

static size_t alarm_text_len(const char *text)
{
    if (!text) {
        return 0;
    }
    return strnlen(text, LS_ALARM_TEXT_MAX_LEN - 1);
}

// alarm_object_t <-> alarm_obj_nvs_t 转换
void alarm_object_to_nvs(const alarm_object_t *alarm, alarm_obj_nvs_t *hdr, size_t *text_len_out) {
    if (!alarm || !hdr) return;
    memset(hdr, 0, sizeof(*hdr));
    hdr->cloud_id = alarm->cloud_id;
    hdr->calendar = (uint8_t)alarm->calendar;
    hdr->trigger_type = (uint8_t)alarm->trigger.type;
    hdr->hour = alarm->trigger.hour;
    hdr->minute = alarm->trigger.minute;
    hdr->second = alarm->trigger.second;
    hdr->year = alarm->trigger.year;
    hdr->month = alarm->trigger.month;
    hdr->day = alarm->trigger.day;
    hdr->day_of_week = alarm->trigger.day_of_week;
    hdr->day_of_month = alarm->trigger.day_of_month;
    hdr->lunar_month = alarm->trigger.lunar_month;
    hdr->lunar_day = alarm->trigger.lunar_day;
    strncpy(hdr->holiday_name, alarm->trigger.holiday_name, sizeof(hdr->holiday_name) - 1);
    hdr->holiday_name[sizeof(hdr->holiday_name) - 1] = '\0';
    hdr->keyword_count = alarm->keyword_count;
    for (uint8_t i = 0; i < ALARM_KEYWORD_MAX; i++) {
        strncpy(hdr->keywords[i], alarm->keywords[i], ALARM_KEYWORD_LEN - 1);
        hdr->keywords[i][ALARM_KEYWORD_LEN - 1] = '\0';
    }
    size_t text_len = alarm_text_len((const char *)alarm->text);
    hdr->text_len = (uint16_t)text_len;
    if (text_len_out) *text_len_out = text_len;
}

void alarm_object_from_nvs(const alarm_obj_nvs_t *hdr, const uint8_t *text, size_t text_len, uint64_t alarm_id, alarm_object_t *out_alarm) {
    if (!hdr || !out_alarm) return;
    memset(out_alarm, 0, sizeof(*out_alarm));
    out_alarm->alarm_id = alarm_id;
    out_alarm->cloud_id = hdr->cloud_id;
    out_alarm->calendar = (alarm_calendar_t)hdr->calendar;
    out_alarm->trigger.type = (alarm_trigger_type_t)hdr->trigger_type;
    out_alarm->trigger.hour = hdr->hour;
    out_alarm->trigger.minute = hdr->minute;
    out_alarm->trigger.second = hdr->second;
    out_alarm->trigger.year = hdr->year;
    out_alarm->trigger.month = hdr->month;
    out_alarm->trigger.day = hdr->day;
    out_alarm->trigger.day_of_week = hdr->day_of_week;
    out_alarm->trigger.day_of_month = hdr->day_of_month;
    out_alarm->trigger.lunar_month = hdr->lunar_month;
    out_alarm->trigger.lunar_day = hdr->lunar_day;
    strncpy(out_alarm->trigger.holiday_name, hdr->holiday_name, sizeof(out_alarm->trigger.holiday_name) - 1);
    out_alarm->trigger.holiday_name[sizeof(out_alarm->trigger.holiday_name) - 1] = '\0';
    out_alarm->keyword_count = hdr->keyword_count;
    for (uint8_t i = 0; i < ALARM_KEYWORD_MAX; i++) {
        strncpy(out_alarm->keywords[i], hdr->keywords[i], ALARM_KEYWORD_LEN - 1);
        out_alarm->keywords[i][ALARM_KEYWORD_LEN - 1] = '\0';
    }
    if (text && text_len > 0) {
        size_t max_copy = sizeof(out_alarm->text) - 1;
        size_t copy_len = text_len > max_copy ? max_copy : text_len;
        memcpy(out_alarm->text, text, copy_len);
        out_alarm->text[copy_len] = '\0';
    }
}

static int alarm_obj_add_to_nvs(const alarm_object_t *alarm)
{
    if (!alarm) {
        return -1;
    }

    alarm_obj_nvs_t hdr;
    size_t text_len = 0;
    alarm_object_to_nvs(alarm, &hdr, &text_len);
    size_t total_len = sizeof(hdr) + text_len;
    uint8_t *buf = lisa_mem_alloc(total_len);
    if (!buf) {
        return -1;
    }
    memcpy(buf, &hdr, sizeof(hdr));
    if (text_len > 0) {
        memcpy(buf + sizeof(hdr), alarm->text, text_len);
    }

    char key[32];
    snprintf(key, sizeof(key), "%llu", (unsigned long long)alarm->alarm_id);
    int ret = lisa_kv_set_blob(key, buf, total_len);
    lisa_mem_free(buf);

    return ret;
}

static int alarm_obj_load_from_nvs(uint64_t alarm_id, alarm_object_t *out_alarm)
{
    if (!out_alarm) {
        return -1;
    }

    char key[32];
    snprintf(key, sizeof(key), "%llu", (unsigned long long)alarm_id);

    uint8_t *data = NULL;
    int len = 0;
    int ret = lisa_kv_get_blob(key, &data, &len);
    if (ret != 0 || !data || len < (int)sizeof(alarm_obj_nvs_t)) {
        if (data) {
            lisa_mem_free(data);
        }
        return -1;
    }

    if ((size_t)len < sizeof(alarm_obj_nvs_t)) {
        lisa_mem_free(data);
        return -1;
    }
    alarm_obj_nvs_t hdr;
    memcpy(&hdr, data, sizeof(hdr));
    size_t text_len = hdr.text_len;
    if ((size_t)len < sizeof(hdr) + text_len) {
        lisa_mem_free(data);
        return -1;
    }
    alarm_object_from_nvs(&hdr, data + sizeof(hdr), text_len, alarm_id, out_alarm);

    lisa_mem_free(data);
    return 0;
}

static int alarm_obj_delete_from_nvs(uint64_t alarm_id)
{
    char key[32];
    snprintf(key, sizeof(key), "%llu", (unsigned long long)alarm_id);
    return lisa_kv_del(key);
}


/* ==================== 闹钟管理工具 ==================== */



int alarm_store_create_obj(const alarm_object_t *alarm, char *err_msg, size_t err_len)
{
    if (!alarm) {
        snprintf(err_msg, err_len, "闹钟参数无效");
        return -1;
    }

    // 时间戳作为闹钟ID
    uint64_t alarm_id = alarm_obj_to_timestamp(alarm);
    if (alarm_id == 0) {
        LISA_LOGE(TAG, "invalid alarm datetime, cannot create alarm");
        snprintf(err_msg, err_len, "闹钟时间无效");
        return -1;
    }

    // 检查闹钟时间是否已过
    time_t now_ts = 0;
    if (get_network_time(NULL, &now_ts) != 0) {
        LISA_LOGE(TAG, "failed to get current time");
        snprintf(err_msg, err_len, "无法获取当前时间");
        return -1;
    }
    if ((time_t)alarm_id <= now_ts) {
        LISA_LOGE(TAG, "alarm time already passed, id=%llu , now = %llu", (unsigned long long)alarm_id, now_ts);
        snprintf(err_msg, err_len, "闹钟时间已过期");
        return -1;
    }

    // 确保ID唯一
    uint64_t *ids = NULL;
    uint32_t count = 0;
    if (alarm_list_load(&ids, &count) != 0) {
        if (ids) {
            lisa_mem_free(ids);
        }
        LISA_LOGE(TAG, "failed to load alarm list");
        snprintf(err_msg, err_len, "闹钟列表读取失败");
        return -1;
    }
    while (alarm_list_contains(ids, count, alarm_id)) {
        LISA_LOGE(TAG, "alarm id %llu already exists",(unsigned long long)alarm_id);
        snprintf(err_msg, err_len, "闹钟已存在");
        return -1;
    }

    // 保存闹钟
    alarm_object_t temp = *alarm;

    // 添加到缓存
    alarm_store_cache_add(&temp);

    // 保存闹钟对象到nvs
    temp.alarm_id = alarm_id;
    LISA_LOGI(TAG, "alarm_store_create_obj called, alarm id=%llu", (unsigned long long)alarm_id);
    int ret = alarm_obj_add_to_nvs(&temp);
    if (ret == 0) {
        ret = alarm_list_add(alarm_id);
    }
    if (ids) {
        lisa_mem_free(ids);
    }


    return ret;
}

int alarm_store_create_obj_by_timestamp(const uint64_t timestamp, const uint8_t *text)
{
    alarm_object_t temp;
    if (alarm_obj_from_timestamp(timestamp, text, &temp) != 0) {
        LISA_LOGE(TAG, "failed to build alarm object from timestamp: %llu",
                  (unsigned long long)timestamp);
        return -1;
    }

    return alarm_store_create_obj(&temp, NULL, 0);
}

int alarm_store_delete_obj(const alarm_object_t *alarm)
{
    if (alarm == NULL) {
        return -1;
    }

    uint64_t target_id = alarm_obj_to_timestamp(alarm);
    if (target_id == 0) {
        LISA_LOGE(TAG, "invalid alarm datetime, cannot update alarm");
        return -1;
    }

    uint64_t *ids = NULL;
    uint32_t count = 0;
    if (alarm_list_load(&ids, &count) != 0) {
        if (ids) {
            lisa_mem_free(ids);
        }
        return -1;
    }

    if (!alarm_list_contains(ids, count, target_id)) {
        if (ids) {
            lisa_mem_free(ids);
        }
        LISA_LOGE(TAG, "alarm id %llu not found", (unsigned long long)target_id);
        return -1;
    }

    int ret = alarm_obj_delete_from_nvs(target_id);
    if (ret == 0) {
        lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
        ret = alarm_list_remove(target_id);
        lisa_mutex_unlock(s_alarm_store_mutex);
    }
    if (ret == 0) {
        alarm_store_cache_remove(target_id);
    }

    if (ids) {
        lisa_mem_free(ids);
    }

    return ret;
}

int alarm_store_delete_obj_by_timestamp(const uint64_t timestamp)
{
    alarm_object_t temp;
    if (alarm_obj_from_timestamp(timestamp, (const uint8_t*)"", &temp) != 0) {
        LISA_LOGE(TAG, "failed to build alarm object from timestamp: %llu",
                  (unsigned long long)timestamp);
        return -1;
    }

    return alarm_store_delete_obj(&temp);
}


int alarm_store_update_obj(const alarm_object_t *alarm)
{
    if (!alarm) {
        return -1;
    }

    uint64_t target_id = alarm_obj_to_timestamp(alarm);
    if (target_id == 0) {
        LISA_LOGE(TAG, "invalid alarm datetime, cannot update alarm");
        return -1;
    }

    alarm_object_t existing;
    if (alarm_obj_load_from_nvs(target_id, &existing) != 0) {
        LISA_LOGE(TAG, "alarm id %llu not found", (unsigned long long)target_id);
        return -1;
    }

    alarm_object_t temp = *alarm;
    temp.alarm_id = target_id;

    LISA_LOGI(TAG, "alarm_store_update_obj called, alarm id=%llu", (unsigned long long)target_id);
    int ret = alarm_obj_add_to_nvs(&temp);
    if (ret == 0) {
        alarm_store_cache_add(&temp);
    }
    return ret;
}

int alarm_store_update_obj_by_timestamp(const uint64_t timestamp, const uint8_t *text)
{
    alarm_object_t temp;
    if (alarm_obj_load_from_nvs(timestamp, &temp) != 0) {
        LISA_LOGE(TAG, "failed to load alarm object: %llu",
                  (unsigned long long)timestamp);
        return -1;
    }

    if (text && text[0] != '\0') {
        strncpy(temp.text, (const char *)text, sizeof(temp.text) - 1);
        temp.text[sizeof(temp.text) - 1] = '\0';
    }

    return alarm_store_update_obj(&temp);
}


// 查询指定闹钟对象是否存在，存在则返回text，不存在返回错误
// text_out: 输出参数，指向缓冲区，长度为buf_len
// 返回值: 0=找到，-1=未找到或出错
int alarm_store_query_obj(const alarm_object_t *alarm, char *text_out, size_t buf_len)
{
    if (!alarm || !text_out || buf_len == 0) {
        return -1;
    }

    uint64_t alarm_id = alarm_obj_to_timestamp(alarm);
    if (alarm_id == 0) {
        return -1;
    }

    alarm_object_t found;
    if (alarm_obj_load_from_nvs(alarm_id, &found) == 0) {
        // 拷贝text到输出缓冲区
        strncpy(text_out, (const char *)found.text, buf_len - 1);
        text_out[buf_len - 1] = '\0';
        return 0;
    }
    
    return -1;
}

// 查询所有闹钟对象
const alarm_store_t *alarm_store_get_all(void) {
    return &s_alarm_store;
}

// 查询指定id的闹钟对象
const alarm_object_t *alarm_store_find_by_id(uint64_t alarm_id) {
    for (uint32_t i = 0; i < s_alarm_store.count; ++i) {
        if (s_alarm_store.items[i].alarm_id == alarm_id) {
            return &s_alarm_store.items[i];
        }
    }
    return NULL;
}

const alarm_object_t *alarm_store_find_by_cloud_id(uint64_t cloud_id) {
    if (cloud_id == 0) {
        return NULL;
    }

    const alarm_object_t *found = NULL;
    lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
    for (uint32_t i = 0; i < s_alarm_store.count; ++i) {
        if (s_alarm_store.items[i].cloud_id == cloud_id) {
            found = &s_alarm_store.items[i];
            break;
        }
    }
    lisa_mutex_unlock(s_alarm_store_mutex);
    return found;
}


/* ==================== 初始化 ==================== */

static int alarm_store_load(alarm_store_t *store)
{
    if (!store) {
        return -1;
    }

    memset(store, 0, sizeof(*store));
    store->count = 0;

    // 读取闹钟ID列表
    uint64_t *ids = NULL;
    uint32_t count = 0;
    int ret = alarm_list_load(&ids, &count);
    for (uint32_t i = 0; i < count; ++i) {
        LISA_LOGI(TAG, "alarm_store_load: id[%u] = %llu", i, (unsigned long long)ids[i]);
    }

    if (ret != 0 || !ids || count == 0) {
        if (ids) {
            lisa_mem_free(ids);
        }
        return 0;
    }
    
    // 加载 alarm_obj 到 alarm_store
    uint32_t loaded = 0;
    for (uint32_t i = 0; i < count && loaded < LS_ALARM_MAX_COUNT; ++i) {
        alarm_object_t obj;
        if (alarm_obj_load_from_nvs(ids[i], &obj) == 0) {
            store->items[loaded] = obj;
            loaded++;
        } else {
            // id没有对应的闹钟对象，从列表中删除
            lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
            alarm_list_remove(ids[i]);
            lisa_mutex_unlock(s_alarm_store_mutex);
        }
    }
    store->count = loaded;
    
    lisa_mem_free(ids);
    return 0;
}

int alarm_store_init(void)
{
    if (s_alarm_store_mutex == NULL) {
		s_alarm_store_mutex = lisa_mutex_create();
	}

    int ret;
    ret = alarm_store_load(&s_alarm_store);

    // 过期闹钟检查与重建
    time_t now_ts = 0;
    bool have_now = (get_network_time(NULL, &now_ts) == 0);
    if (have_now) {
        uint32_t new_count = 0;
        for (uint32_t i = 0; i < s_alarm_store.count; ++i) {
            alarm_object_t obj = s_alarm_store.items[i];
            if (obj.alarm_id <= (uint64_t)now_ts) {
                if (obj.trigger.type == ALARM_TRIG_ONCE) { // 单次闹钟过期直接清除
                    lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
                    alarm_list_remove(obj.alarm_id);
                    lisa_mutex_unlock(s_alarm_store_mutex);
                    alarm_obj_delete_from_nvs(obj.alarm_id);
                    continue;
                }
                // 循环闹钟计算下次触发时间，并更新nvs以及store
                alarm_object_t temp = obj;
                uint64_t next_ts = alarm_calc_next_trigger(&temp, now_ts);
                if (next_ts > (uint64_t)now_ts) {
                    temp.alarm_id = next_ts;
                    alarm_obj_delete_from_nvs(obj.alarm_id);
                    lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
                    alarm_list_remove(obj.alarm_id);
                    alarm_list_add(next_ts);
                    lisa_mutex_unlock(s_alarm_store_mutex);
                    alarm_obj_add_to_nvs(&temp);
                    s_alarm_store.items[new_count++] = temp;
                } else {
                    lisa_mutex_lock(s_alarm_store_mutex, LISA_OS_WAIT_FOREVER);
                    alarm_list_remove(obj.alarm_id);
                    lisa_mutex_unlock(s_alarm_store_mutex);
                    alarm_obj_delete_from_nvs(obj.alarm_id);
                }
            } else {
                s_alarm_store.items[new_count++] = obj;
            }
        }
        s_alarm_store.count = new_count;
    }

    // 遍历所有已加载的闹钟对象，创建定时器
    for (uint32_t i = 0; i < s_alarm_store.count; ++i) {
        alarm_object_t *alarm = &s_alarm_store.items[i];
        const uint8_t *text = (alarm->text[0] != '\0') ? (const uint8_t *)alarm->text : (const uint8_t *)"";
        ls_alarm_insert_by_timestamp(alarm->alarm_id, text);
    }

    return 0;
}
