#define TAG "alarm_nvs"

#include "alarm_nvs.h"
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
#include "shell.h"

#include "alarm.h"
#include "alarm_handler.h"
#include "alarm_next.h"
#include "alarm_time_utils.h"

static lisa_mutex_t *s_alarm_nvs_mutex = NULL;

/* ==================== 打印闹钟详情 ==================== */
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
    case ALARM_TRIG_CUSTOM:
        return "CUSTOM";
    default:
        return "UNKNOWN";
    }
}
void alarm_obj_print(const alarm_object_t *alarm)
{
    if (!alarm) {
        LISA_LOGW(TAG, "alarm_obj_print: null alarm");
        return;
    }

    char buf[1024];  // 增大缓冲区以避免截断
    size_t used = 0;

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
    if (alarm->trigger.month_of_year) {
        APPEND_FIELD(" month_of_year=%u", (unsigned)alarm->trigger.month_of_year);
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

    // 打印 snooze 配置
    if (alarm->snooze_enabled) {
        APPEND_FIELD(" snooze=on interval=%umin count=%u",
                     (unsigned)alarm->snooze_interval,
                     (unsigned)alarm->snooze_count);
    } else {
        APPEND_FIELD("%s", " snooze=off");
    }

    // 同时输出到 shell（adb shell 可以看到）
    Shell *shell = shellGetCurrent();
    if (shell && shell->write) {
        shell->write(buf, used);
        shell->write("\r\n", 2);  // 添加换行
    }
#undef APPEND_FIELD
}

/* ==================== 列表操作 ==================== */

static int alarm_list_load(alarm_list_item_t **items, uint32_t *count)
{
    int len = 0;
    uint8_t *data = NULL;

    if (items) {
        *items = NULL;
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

    if (len % (int)sizeof(alarm_list_item_t) != 0) {
        lisa_mem_free(data);
        return -1;
    }

    if (items) {
        *items = (alarm_list_item_t *)data;
    } else {
        lisa_mem_free(data);
        return -1;
    }

    if (count) {
        *count = (uint32_t)(len / sizeof(alarm_list_item_t));
    }

    return 0;
}

static int alarm_list_save(const alarm_list_item_t *items, uint32_t count)
{
    if (!items || count == 0) {
        return lisa_kv_set_blob(ALARM_LIST_KEY, (uint8_t *)items, 0);
    }
    return lisa_kv_set_blob(ALARM_LIST_KEY, (uint8_t *)items, count * sizeof(alarm_list_item_t));
}

static int alarm_list_add(uint64_t cloud_id)
{
    alarm_list_item_t *items = NULL;
    uint32_t count = 0;
    int ret = alarm_list_load(&items, &count);
    if (ret != 0) {
        if (items) lisa_mem_free(items);
        return -1;
    }

    // 检查是否已存在
    for (uint32_t i = 0; i < count; ++i) {
        if (items[i].cloud_id == cloud_id) {
            lisa_mem_free(items);
            return 0; // 已存在
        }
    }

    // 添加新项
    alarm_list_item_t *new_items = lisa_mem_alloc((count + 1) * sizeof(alarm_list_item_t));
    if (!new_items) {
        if (items) lisa_mem_free(items);
        return -1;
    }
    if (count > 0 && items) {
        memcpy(new_items, items, count * sizeof(alarm_list_item_t));
    }
    new_items[count].cloud_id = cloud_id;

    ret = alarm_list_save(new_items, count + 1);
    lisa_mem_free(new_items);
    if (items) lisa_mem_free(items);
    return ret;
}

static int alarm_list_remove(uint64_t cloud_id)
{
    alarm_list_item_t *items = NULL;
    uint32_t count = 0;
    int ret = alarm_list_load(&items, &count);
    if (ret != 0 || count == 0) {
        if (items) lisa_mem_free(items);
        return -1;
    }

    uint32_t new_count = 0;
    alarm_list_item_t *new_items = lisa_mem_alloc(count * sizeof(alarm_list_item_t));
    if (!new_items) {
        if (items) lisa_mem_free(items);
        return -1;
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (items[i].cloud_id != cloud_id) {
            new_items[new_count++] = items[i];
        }
    }

    if (new_count == 0) {
        ret = lisa_kv_del(ALARM_LIST_KEY);
    } else {
        ret = lisa_kv_set_blob(ALARM_LIST_KEY, (uint8_t *)new_items, new_count * sizeof(alarm_list_item_t));
    }

    lisa_mem_free(new_items);
    if (items) lisa_mem_free(items);
    return ret;
}

/* ==================== 公开接口 ==================== */

int alarm_nvs_init(void)
{
    if (s_alarm_nvs_mutex == NULL) {
        s_alarm_nvs_mutex = lisa_mutex_create();
    }

    // 加载闹钟列表
    alarm_list_item_t *items = NULL;
    uint32_t count = 0;
    if (alarm_list_load(&items, &count) != 0) {
        LISA_LOGW(TAG, "failed to load alarm list");
        return 0;
    }

    if (!items || count == 0) {
        return 0;
    }

    LISA_LOGI(TAG, "alarm_nvs_init: %u alarms in NVS", count);

    // 获取当前时间用于过期检查
    int64_t now_ts = alarm_time_get_network_timestamp();
    bool have_now = (now_ts >= 0);

    // 遍历所有闹钟
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t cloud_id = items[i].cloud_id;
        alarm_object_t obj;

        // 从 NVS 加载闹钟对象
        char key[32];
        snprintf(key, sizeof(key), "%llu", (unsigned long long)cloud_id);

        uint8_t *data = NULL;
        int len = 0;
        if (lisa_kv_get_blob(key, &data, &len) != 0 || !data || len != sizeof(alarm_object_t)) {
            if (data) lisa_mem_free(data);
            LISA_LOGW(TAG, "failed to load alarm cloud_id=%llu, removing", (unsigned long long)cloud_id);
            alarm_list_remove(cloud_id);
            continue;
        }

        memcpy(&obj, data, sizeof(alarm_object_t));
        lisa_mem_free(data);

        uint64_t alarm_id = obj.alarm_id;

        LISA_LOGI(TAG, "processing alarm cloud_id=%llu, alarm_id=%llu, type=%d, now=%lld",
                  (unsigned long long)cloud_id, (unsigned long long)alarm_id,
                  obj.trigger.type, (long long)now_ts);

        // 检查是否过期
        if (have_now && alarm_id <= (uint64_t)now_ts) {
            LISA_LOGI(TAG, "alarm cloud_id=%llu is expired (alarm_id=%llu <= now=%lld)",
                      (unsigned long long)cloud_id, (unsigned long long)alarm_id, (long long)now_ts);

            if (obj.trigger.type == ALARM_TRIG_ONCE) {
                // 单次闹钟过期直接删除
                LISA_LOGI(TAG, "removing expired once alarm cloud_id=%llu", (unsigned long long)cloud_id);
                lisa_kv_del(key);
                alarm_list_remove(cloud_id);
                continue;
            }

            // 循环闹钟计算下次触发时间
            uint64_t next_ts = alarm_calc_next_trigger(&obj, now_ts);
            LISA_LOGI(TAG, "recurring alarm cloud_id=%llu next_ts=%llu",
                      (unsigned long long)cloud_id, (unsigned long long)next_ts);

            if (next_ts > (uint64_t)now_ts) {
                LISA_LOGI(TAG, "recurring alarm cloud_id=%llu next trigger: %llu",
                          (unsigned long long)cloud_id, (unsigned long long)next_ts);

                obj.alarm_id = next_ts;
                lisa_kv_del(key);
                alarm_list_remove(cloud_id);
                alarm_list_add(cloud_id);
                lisa_kv_set_blob(key, (uint8_t *)&obj, sizeof(alarm_object_t));

                alarm_id = next_ts;
            } else {
                LISA_LOGW(TAG, "recurring alarm cloud_id=%llu has no future trigger (next_ts=%llu), removing",
                          (unsigned long long)cloud_id, (unsigned long long)next_ts);
                lisa_kv_del(key);
                alarm_list_remove(cloud_id);
                continue;
            }
        }

        // 插入运行时链表
        LISA_LOGI(TAG, "inserting alarm to runtime list, cloud_id=%llu, alarm_id=%llu",
                  (unsigned long long)cloud_id, (unsigned long long)alarm_id);
        const uint8_t *text = (obj.text[0] != '\0') ?
                              (const uint8_t *)obj.text : (const uint8_t *)"";
        ls_alarm_insert_by_timestamp(alarm_id, obj.cloud_id, text);
    }

    lisa_mem_free(items);
    return 0;
}

int alarm_nvs_add(const alarm_object_t *alarm)
{
    if (!alarm || alarm->cloud_id == 0 || alarm->alarm_id == 0) {
        return -1;
    }

    // 保存闹钟对象（key 是 cloud_id）
    char key[32];
    snprintf(key, sizeof(key), "%llu", (unsigned long long)alarm->cloud_id);

    LISA_LOGI(TAG, "alarm_nvs_add: cloud_id=%llu, alarm_id=%llu",
              (unsigned long long)alarm->cloud_id, (unsigned long long)alarm->alarm_id);

    if (lisa_kv_set_blob(key, (uint8_t *)alarm, sizeof(alarm_object_t)) != 0) {
        return -1;
    }

    // 添加到列表
    return alarm_list_add(alarm->cloud_id);
}

int alarm_nvs_delete(uint64_t cloud_id)
{
    if (cloud_id == 0) {
        LISA_LOGE(TAG, "invalid cloud_id");
        return -1;
    }

    alarm_handler_cancel_snooze_by_cloud_id(cloud_id);

    // 先查询对象获取 alarm_id
    const alarm_object_t *alarm = alarm_nvs_find(cloud_id);
    if (!alarm) {
        LISA_LOGW(TAG, "alarm not found for cloud_id=%llu", (unsigned long long)cloud_id);
        return -1;
    }

    uint64_t alarm_id = alarm->alarm_id;

    // 删除 NVS 对象
    char key[32];
    snprintf(key, sizeof(key), "%llu", (unsigned long long)cloud_id);

    if (lisa_kv_del(key) != 0) {
        LISA_LOGE(TAG, "failed to delete alarm from NVS, cloud_id=%llu", (unsigned long long)cloud_id);
        return -1;
    }

    // 从列表移除
    if (alarm_list_remove(cloud_id) != 0) {
        LISA_LOGE(TAG, "failed to remove alarm from list, cloud_id=%llu", (unsigned long long)cloud_id);
        return -1;
    }

    // 从运行时链表删除
    ls_alarm_delete_by_timestamp(alarm_id);

    LISA_LOGI(TAG, "alarm deleted: cloud_id=%llu, alarm_id=%llu",
              (unsigned long long)cloud_id, (unsigned long long)alarm_id);
    return 0;
}

int alarm_nvs_update(const alarm_object_t *alarm)
{
    if (!alarm || alarm->cloud_id == 0) {
        return -1;
    }

    // 先删除
    alarm_nvs_delete(alarm->cloud_id);

    // 再创建
    return alarm_nvs_add(alarm);
}

const alarm_object_t *alarm_nvs_find(uint64_t cloud_id)
{
    if (cloud_id == 0) {
        return NULL;
    }

    char key[32];
    snprintf(key, sizeof(key), "%llu", (unsigned long long)cloud_id);

    uint8_t *data = NULL;
    int len = 0;
    if (lisa_kv_get_blob(key, &data, &len) != 0 || !data) {
        return NULL;
    }

    static alarm_object_t temp __psram_bss__;
    memset(&temp, 0, sizeof(alarm_object_t));  // 清零，确保新字段有默认值

    // 兼容性处理：支持旧版本数据
    if (len <= sizeof(alarm_object_t)) {
        memcpy(&temp, data, len);  // 复制实际数据
        LISA_LOGI(TAG, "alarm loaded: cloud_id=%llu, data_len=%d, struct_len=%zu",
                  (unsigned long long)cloud_id, len, sizeof(alarm_object_t));
    } else {
        LISA_LOGW(TAG, "alarm data too large: cloud_id=%llu, data_len=%d, struct_len=%zu",
                  (unsigned long long)cloud_id, len, sizeof(alarm_object_t));
        lisa_mem_free(data);
        return NULL;
    }

    lisa_mem_free(data);
    return &temp;
}

int alarm_nvs_count(void)
{
    alarm_list_item_t *items = NULL;
    uint32_t count = 0;

    if (alarm_list_load(&items, &count) != 0) {
        return 0;
    }

    if (items) {
        lisa_mem_free(items);
    }

    return (int)count;
}

int alarm_nvs_get_all(alarm_object_t *alarms, int max_count)
{
    if (!alarms || max_count <= 0) {
        return -1;
    }

    alarm_list_item_t *items = NULL;
    uint32_t count = 0;

    if (alarm_list_load(&items, &count) != 0) {
        return 0;
    }

    if (!items || count == 0) {
        return 0;
    }

    int loaded = 0;
    for (uint32_t i = 0; i < count && loaded < max_count; i++) {
        uint64_t cloud_id = items[i].cloud_id;
        const alarm_object_t *obj = alarm_nvs_find(cloud_id);
        if (obj) {
            memcpy(&alarms[loaded], obj, sizeof(alarm_object_t));
            loaded++;
        }
    }

    lisa_mem_free(items);
    return loaded;
}
