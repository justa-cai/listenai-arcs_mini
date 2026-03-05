#include "service_alarm.h"
#include "alarm.h"
#include "alarm_store.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "voice_msg.h"

#include <string.h>
#include <stdio.h>

#define TAG "service_alarm"

static uint8_t alarm_init_done = 0;

static void service_alarm_set_err(char *err_msg, size_t err_len, const char *text)
{
    if (!err_msg || err_len == 0 || !text) {
        return;
    }
    snprintf(err_msg, err_len, "%s", text);
}

static void service_alarm_publish_create(uint64_t timestamp, const uint8_t *text)
{
    struct service_alarm alarm = {0};
    alarm.timestamp = timestamp;
    if (text && text[0] != '\0') {
        int cpy_len = strlen((const char *)text) > sizeof(alarm.text) - 1 ? sizeof(alarm.text) - 1 : strlen((const char *)text);
        memcpy(alarm.text, text, cpy_len);
    }
    voice_msg_pub(VOICE_MSG_ALARM_CREATE, &alarm, sizeof(struct service_alarm));
}

static void service_alarm_publish_delete(uint64_t timestamp)
{
    struct service_alarm alarm = {0};
    alarm.timestamp = timestamp;
    voice_msg_pub(VOICE_MSG_ALARM_DELETE, &alarm, sizeof(struct service_alarm));
}


int service_alarm_create_obj(const alarm_object_t *alarm, char *err_msg, size_t err_len)
{
    if (!alarm) {
        service_alarm_set_err(err_msg, err_len, "闹钟参数无效");
        return -1;
    }

    int ret = alarm_store_create_obj(alarm, err_msg, err_len);
    if (ret != 0) {
        return -1;
    }

    uint64_t timestamp = alarm_obj_to_timestamp(alarm);
    if (timestamp == 0) {
        service_alarm_set_err(err_msg, err_len, "闹钟时间无效");
        alarm_store_delete_obj(alarm);
        return -1;
    }

    ret = ls_alarm_insert_by_timestamp(timestamp, (const uint8_t *)alarm->text);
    if (ret != 0) {
        alarm_store_delete_obj(alarm);
        service_alarm_set_err(err_msg, err_len, "创建闹钟失败，操作闹钟链表时出现异常");
        return -1;
    }

    service_alarm_publish_create(timestamp, (const uint8_t *)alarm->text);
    return 0;
}

const alarm_object_t *service_alarm_find_by_cloud_id(uint64_t cloud_id)
{
    return alarm_store_find_by_cloud_id(cloud_id);
}

int service_alarm_delete_by_cloud_id(uint64_t cloud_id, char *err_msg, size_t err_len)
{
    const alarm_object_t *target = alarm_store_find_by_cloud_id(cloud_id);
    if (!target) {
        service_alarm_set_err(err_msg, err_len, "未找到对应闹钟");
        return -1;
    }

    alarm_object_t backup = *target;

    if (alarm_store_delete_obj(&backup) != 0) {
        service_alarm_set_err(err_msg, err_len, "删除闹钟失败,清除nvs时出现异常");
        return -1;
    }

    if (ls_alarm_delete_by_timestamp(backup.alarm_id) != 0) {
        service_alarm_set_err(err_msg, err_len, "删除闹钟失败，操作闹钟链表时出现异常");
        return -1;
    }

    service_alarm_publish_delete(backup.alarm_id);
    return 0;
}


static void alarm_event_handler(uint64_t timestamp, const uint8_t *text)
{
    struct service_alarm alarm = {0};
    alarm.timestamp = timestamp;
    int cpy_len = strlen(text) > sizeof(alarm.text) - 1 ? sizeof(alarm.text) - 1 : strlen(text);
    memcpy(alarm.text, text, cpy_len);
    LOGI("service alarm, alarm triger, timestamp: %llu", timestamp);
    voice_msg_pub(VOICE_MSG_ALARM_TRIGGER, &alarm, sizeof(struct service_alarm));
}

void service_alarm_init(void)
{
    if (alarm_init_done) {
        return;
    }

    ls_alarm_init(alarm_event_handler);

    alarm_init_done = 1;
    
    uint32_t cnt = (uint32_t)ls_alarm_count_get();
    voice_msg_pub(VOICE_MSG_ALARM_QUERY, &cnt, sizeof(cnt));
    LOGI("service_alarm_init publish alarm count: %u", (unsigned int)cnt);
}

uint8_t service_alarm_init_done(void)
{
    return alarm_init_done;
}

int service_alarm_add(uint64_t timestamp, const uint8_t *text)
{
    int ret = ls_alarm_insert_by_timestamp(timestamp, text);
    if (ret == 0) {
        LOGI("Alarm added - timestamp: %llu", (unsigned long long)timestamp);
        struct service_alarm alarm = {0};
        alarm.timestamp = timestamp;
        int cpy_len = strlen(text) > sizeof(alarm.text) - 1 ? sizeof(alarm.text) - 1 : strlen(text);
        memcpy(alarm.text, text, cpy_len);
        voice_msg_pub(VOICE_MSG_ALARM_CREATE, &alarm, sizeof(struct service_alarm));
    } else {
        LOGE("Failed to add alarm - timestamp: %llu, ret: %d", (unsigned long long)timestamp, ret);
    }

    return ret;
}

int service_alarm_delete(uint64_t timestamp)
{
    int ret = ls_alarm_delete_by_timestamp(timestamp);
    if (ret == 0) {
        LOGI("Alarm added - timestamp: %llu", (unsigned long long)timestamp);
        struct service_alarm alarm = {0};
        alarm.timestamp = timestamp;
        voice_msg_pub(VOICE_MSG_ALARM_DELETE, &alarm, sizeof(struct service_alarm));
    } else {
        LOGE("Failed to delete alarm - timestamp: %llu, ret: %d", (unsigned long long)timestamp, ret);
    }
    return ret;
}

int service_alarm_get_count(void)
{
    return ls_alarm_count_get();
}

int service_alarm_delete_all(uint32_t cnt)
{
    struct ls_alarm *alarm_node = ls_alarm_get();
    int deleted_count = 0;
    int failed_count = 0;

    // Iterate through all alarms and delete them
    while (alarm_node != NULL) {
        uint64_t timestamp = alarm_node->timestamp;
        // Move to next before deleting current node
        alarm_node = alarm_node->next;

        int ret = ls_alarm_delete_by_timestamp(timestamp);
        if (ret == 0) {
            deleted_count++;
        } else {
            failed_count++;
            LOGE("Failed to delete alarm - timestamp: %llu", (unsigned long long)timestamp);
        }
    }

    if (deleted_count > 0) {
        LOGI("Deleted %d alarm(s)", deleted_count);
    }

    if (failed_count > 0) {
        LOGW("Failed to delete %d alarm(s)", failed_count);
        return -1;
    }

    return 0;
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
