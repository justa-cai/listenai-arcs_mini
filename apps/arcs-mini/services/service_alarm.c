#define TAG "service_alarm"

#include <string.h>
#include <stdio.h>

#include "lisa_log.h"
#include "lisa_mem.h"

#include "voice_msg.h"
#include "service_alarm.h"
#include "alarm.h"
#include "alarm_nvs.h"
#include "alarm_api.h"
#include "lisa_thread.h"

static uint8_t alarm_init_done = 0;

/* ===========================================================================*/
/* 发布闹钟操作消息，更新UI                                                       */
/* ============================================================================*/

void service_alarm_event_init(struct service_alarm *alarm,
                              uint64_t timestamp,
                              const uint8_t *text)
{
    if (!alarm) {
        return;
    }

    memset(alarm, 0, sizeof(*alarm));
    alarm->timestamp = timestamp;

    if (text && text[0] != '\0') {
        int cpy_len = strlen((const char *)text) > sizeof(alarm->text) - 1 ? sizeof(alarm->text) - 1 : strlen((const char *)text);
        memcpy(alarm->text, text, cpy_len);
    }
}

static void service_alarm_publish_create(uint64_t timestamp, const uint8_t *text)
{
    struct service_alarm alarm = {0};
    service_alarm_event_init(&alarm, timestamp, text);
    voice_msg_pub(VOICE_MSG_ALARM_CREATE, &alarm, sizeof(struct service_alarm));
}

static void service_alarm_publish_delete(uint64_t timestamp)
{
    struct service_alarm alarm = {0};
    service_alarm_event_init(&alarm, timestamp, NULL);
    voice_msg_pub(VOICE_MSG_ALARM_DELETE, &alarm, sizeof(struct service_alarm));
}


/* ===========================================================================*/
/* 闹钟操作接口                                                                */
/* ============================================================================*/

static void service_alarm_set_err(char *err_msg, size_t err_len, const char *text)
{
    if (!err_msg || err_len == 0 || !text) {
        return;
    }
    snprintf(err_msg, err_len, "%s", text);
}


int service_alarm_create(const alarm_object_t *alarm, char *err_msg, size_t err_len)
{
    if (!alarm) {
        service_alarm_set_err(err_msg, err_len, "闹钟参数无效");
        return -1;
    }

    // 1. 闹钟对象添加到 NVS
    if (alarm_nvs_add(alarm) != 0) {
        service_alarm_set_err(err_msg, err_len, "添加闹钟到存储失败");
        return -1;
    }

    // 2. 插入运行时闹钟链表
    if (ls_alarm_insert_by_timestamp(alarm->alarm_id, alarm->cloud_id, (const uint8_t *)alarm->text) != 0) {
        alarm_nvs_delete(alarm->cloud_id);
        service_alarm_set_err(err_msg, err_len, "添加闹钟到运行时链表失败");
        return -1;
    }

    service_alarm_publish_create(alarm->alarm_id, (const uint8_t *)alarm->text);
    return 0;
}

int service_alarm_create_from_params(const alarm_construct_params_t *params, char *err_msg, size_t err_len)
{
    if (!params) {
        service_alarm_set_err(err_msg, err_len, "参数无效");
        return -1;
    }

    // 调用 Domain 层构造对象
    alarm_object_t alarm;
    if (alarm_construct(params, &alarm) != 0) {
        service_alarm_set_err(err_msg, err_len, "闹钟对象构造失败");
        return -1;
    }

    // 调用核心创建接口
    return service_alarm_create(&alarm, err_msg, err_len);
}

const alarm_object_t *service_alarm_find_by_cloud_id(uint64_t cloud_id)
{
    return alarm_nvs_find(cloud_id);
}

int service_alarm_delete_by_cloud_id(uint64_t cloud_id, char *err_msg, size_t err_len)
{
    if (alarm_nvs_delete(cloud_id) != 0) {
        service_alarm_set_err(err_msg, err_len, "删除闹钟失败");
        return -1;
    }

    service_alarm_publish_delete(cloud_id);
    return 0;
}

/* ===========================================================================*/
/* 闹钟初始化接口                                                                */
/* ============================================================================*/

static void alarm_event_handler(uint64_t timestamp, const uint8_t *text)
{
    struct service_alarm alarm = {0};
    service_alarm_event_init(&alarm, timestamp, text);
    LOGI("service alarm, alarm triger, timestamp: %llu", timestamp);

    voice_msg_pub(VOICE_MSG_ALARM_TRIGGER, &alarm, sizeof(struct service_alarm));
}

static void service_alarm_sync_from_cloud(void *arg)
{
    // 从云端同步闹钟
    LOGI("syncing alarms from cloud...");
    int sync_ret = alarm_sync_from_cloud();
    if (sync_ret == 0) {
        LOGI("alarm sync completed successfully");
    } else {
        LOGW("alarm sync failed: %d", sync_ret);
    }

    // 再初始化闹钟系统（读取 NVS、加载链表、启动定时器）
    ls_alarm_init(alarm_event_handler);

    alarm_init_done = 1;

    uint32_t cnt = (uint32_t)ls_alarm_count_get();
    voice_msg_pub(VOICE_MSG_ALARM_QUERY, &cnt, sizeof(cnt));
    LOGI("service_alarm_init publish alarm count: %u", (unsigned int)cnt);

    // 任务完成后删除线程
    lisa_thread_delete(NULL);
}


void service_alarm_init(void)
{
    if (alarm_init_done) {
        return;
    }

    // 创建后台线程异步同步云端闹钟（参考OTA实现）
    lisa_thread_attr_t attr = {
        .name = "alarm_sync",
        .stack_size = 8 * 1024,  // HTTP请求和JSON解析需要较大栈空间
        .priority = 5,           // 中等优先级，不阻塞关键任务
    };
    lisa_thread_t *thread = lisa_thread_create(&attr, service_alarm_sync_from_cloud, NULL);
    if (!thread) {
        LOGE("Failed to create alarm sync thread, fallback to sync mode");
    }
}

uint8_t service_alarm_init_done(void)
{
    return alarm_init_done;
}

void service_alarm_deinit(void)
{
    alarm_init_done = 0;

    LOGI("service_alarm_deinit: mark for re-initialization");
}

struct service_alarm *service_alarm_get_all(uint32_t *cnt)
{
    if (cnt == NULL) {
        return NULL;
    }

    // Get the alarm count
    int alarm_count = ls_alarm_count_get();
    *cnt = alarm_count;

    if (alarm_count == 0) {
        return NULL;
    }

    // Allocate memory for the service_alarm array
    struct service_alarm *alarms = lisa_mem_alloc(sizeof(struct service_alarm) * alarm_count);
    if (alarms == NULL) {
        LOGE("Failed to allocate memory for alarms array");
        *cnt = 0;
        return NULL;
    }
    memset(alarms, 0, sizeof(struct service_alarm) * alarm_count);

    // Get the alarm linked list head
    struct ls_alarm *alarm_node = ls_alarm_get();

    // Convert linked list to array
    int i = 0;
    while (alarm_node != NULL && i < alarm_count) {
        alarms[i].timestamp = alarm_node->timestamp;
        memset(alarms[i].text, 0, sizeof(alarms[i].text));
        int cpy_len = strlen(alarm_node->text) > sizeof(alarms[i].text) - 1
                      ? sizeof(alarms[i].text) - 1
                      : strlen(alarm_node->text);
        memcpy(alarms[i].text, alarm_node->text, cpy_len);

        alarm_node = alarm_node->next;
        i++;
    }

    return alarms;
}
