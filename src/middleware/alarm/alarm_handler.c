#define TAG "alarm_handler"

#include "alarm_handler.h"
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "lisa_timer.h"
#include "lisa_log.h"
#include "voice_msg.h"

#include "alarm_nvs.h"
#include "alarm_next.h"
#include "alarm_ring.h"
#include "alarm_time_utils.h"
#include "alarm_api.h"

struct alarm_ui_service_alarm_msg {
    uint64_t timestamp;
    uint8_t text[LS_ALARM_TEXT_MAX_LEN];
};

#define ALARM_WORK_QUEUE_LEN   LS_ALARM_MAX_COUNT
#define ALARM_WORK_STACK_SIZE  4096
#define ALARM_WORK_PRIORITY    4

typedef enum {
    ALARM_WORK_TRIGGERED = 0,
    ALARM_WORK_STOP_AND_NEXT,
} alarm_work_type_t;

typedef struct {
    alarm_work_type_t type;
    uint64_t fired_ts;
    uint64_t cloud_id;
    int64_t now_ts;
    bool publish_result;
} alarm_work_item_t;

static QueueHandle_t g_alarm_work_queue = NULL;
static TaskHandle_t g_alarm_work_task = NULL;

/* ==================== Snooze 稍后提醒状态管理 ==================== */

static struct {
    uint64_t ringing_cloud_id;      // 当前响铃的闹钟 cloud_id
    uint8_t snooze_current_count;   // 当前已提醒次数
    uint8_t snooze_max_count;       // 最大提醒次数
    uint8_t snooze_interval;        // 提醒间隔（分钟）
    lisa_timer_t *snooze_timer;     // snooze 定时器
    bool snooze_mode_active;        // snooze模式是否激活
} g_snooze_ctx = {0};

static ls_alarm_user_callback_t g_user_callback = NULL;

/* forward declarations for functions used before their definitions */
static void alarm_snooze_stop(void);
static void alarm_handle_next_trigger(const alarm_object_t *alarm_obj, uint64_t fired_ts, time_t now_ts);
static void alarm_handle_stop_and_next_work(uint64_t cloud_id, bool publish_result);
static void alarm_process_triggered_work(uint64_t fired_ts, uint64_t cloud_id, int64_t now_ts);

static void alarm_publish_action_result(voice_msg_alarm_action_result_type_t type)
{
    voice_msg_alarm_action_result_t result = {
        .type = (uint32_t)type,
    };

    voice_msg_pub(VOICE_MSG_ALARM_ACTION_RESULT, &result, sizeof(result));
}

static void alarm_publish_create_msg(const alarm_object_t *alarm_obj)
{
    if (!alarm_obj) {
        return;
    }

    struct alarm_ui_service_alarm_msg msg = {0};
    msg.timestamp = alarm_obj->alarm_id;
    strncpy((char *)msg.text, alarm_obj->text, sizeof(msg.text) - 1);
    voice_msg_pub(VOICE_MSG_ALARM_CREATE, &msg, sizeof(msg));
}

static int alarm_submit_work(const alarm_work_item_t *item)
{
    if (!item || !g_alarm_work_queue) {
        LISA_LOGE(TAG, "alarm work queue not ready");
        return -1;
    }

    if (xQueueSend(g_alarm_work_queue, item, 0) != pdTRUE) {
        LISA_LOGE(TAG, "alarm work queue full, type=%d, cloud_id=%llu",
                  item->type, (unsigned long long)item->cloud_id);
        return -1;
    }

    return 0;
}

static void alarm_work_task(void *arg)
{
    (void)arg;
    alarm_work_item_t item;

    while (1) {
        if (xQueueReceive(g_alarm_work_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (item.type) {
        case ALARM_WORK_TRIGGERED:
            alarm_process_triggered_work(item.fired_ts, item.cloud_id, item.now_ts);
            break;
        case ALARM_WORK_STOP_AND_NEXT:
            alarm_handle_stop_and_next_work(item.cloud_id, item.publish_result);
            break;
        default:
            LISA_LOGW(TAG, "unknown alarm work type=%d", item.type);
            break;
        }
    }
}

static int alarm_work_init(void)
{
    if (g_alarm_work_queue) {
        return 0;
    }

    g_alarm_work_queue = xQueueCreate(ALARM_WORK_QUEUE_LEN, sizeof(alarm_work_item_t));
    if (!g_alarm_work_queue) {
        LISA_LOGE(TAG, "failed to create alarm work queue");
        return -1;
    }

    BaseType_t ret = xTaskCreate(alarm_work_task,
                                 "alarm.work",
                                 ALARM_WORK_STACK_SIZE,
                                 NULL,
                                 ALARM_WORK_PRIORITY,
                                 &g_alarm_work_task);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "failed to create alarm work task");
        vQueueDelete(g_alarm_work_queue);
        g_alarm_work_queue = NULL;
        return -1;
    }

    LISA_LOGI(TAG, "alarm work task created");
    return 0;
}

static int alarm_submit_stop_and_next_work(uint64_t cloud_id, bool publish_result)
{
    alarm_work_item_t item = {
        .type = ALARM_WORK_STOP_AND_NEXT,
        .cloud_id = cloud_id,
        .publish_result = publish_result,
    };

    return alarm_submit_work(&item);
}

/* ==================== Snooze 内部函数 ==================== */

static void alarm_snooze_timer_callback(struct lisa_timer *timer)
{
    (void)timer;

    if (!g_snooze_ctx.snooze_mode_active) {
        LISA_LOGW(TAG, "snooze timer fired but not in snooze mode");
        return;
    }

    g_snooze_ctx.snooze_current_count++;

    LISA_LOGI(TAG, "snooze ring %d/%d, interval=%dmin",
              g_snooze_ctx.snooze_current_count,
              g_snooze_ctx.snooze_max_count,
              g_snooze_ctx.snooze_interval);

    // 触发响铃
    const alarm_object_t *alarm_obj = alarm_nvs_find(g_snooze_ctx.ringing_cloud_id);
    if (alarm_obj && g_user_callback) {
        g_user_callback(alarm_obj->alarm_id, (const uint8_t *)alarm_obj->text);
    }

    // 如果达到最大次数，停止 snooze 并处理下一个触发
    if (g_snooze_ctx.snooze_current_count >= g_snooze_ctx.snooze_max_count) {
        LISA_LOGI(TAG, "snooze reached max count, stopping and advancing alarm");
    }

    // 未达到最大次数，重新启动/重设定时器以在下一间隔继续响铃
    if (g_snooze_ctx.snooze_timer) {
        lisa_timer_change_period(g_snooze_ctx.snooze_timer, g_snooze_ctx.snooze_interval * 60 * 1000);
        lisa_timer_start(g_snooze_ctx.snooze_timer);
    }
}

static void alarm_snooze_prepare(uint64_t cloud_id, uint8_t interval_min, uint8_t max_count)
{
    // 停止之前的 snooze 定时器
    if (g_snooze_ctx.snooze_timer) {
        lisa_timer_stop(g_snooze_ctx.snooze_timer);
    }

    // 保存配置，但不启动定时器
    g_snooze_ctx.ringing_cloud_id = cloud_id;
    g_snooze_ctx.snooze_current_count = 0;
    g_snooze_ctx.snooze_max_count = max_count;
    g_snooze_ctx.snooze_interval = interval_min;
    g_snooze_ctx.snooze_mode_active = false;  // 等待用户按键才激活

    // 创建 snooze 定时器（如果不存在）
    if (!g_snooze_ctx.snooze_timer) {
        g_snooze_ctx.snooze_timer = lisa_timer_create(
            interval_min * 60 * 1000,  // 分钟转毫秒
            alarm_snooze_timer_callback,
            NULL
        );
    }

    LISA_LOGI(TAG, "snooze prepared: cloud_id=%llu, interval=%dmin, max_count=%d (waiting for user action)",
              (unsigned long long)cloud_id, interval_min, max_count);
}

static void alarm_snooze_start_timer(void)
{
    if (!g_snooze_ctx.snooze_timer) {
        LISA_LOGE(TAG, "snooze timer not created");
        return;
    }

    g_snooze_ctx.snooze_mode_active = true;

    // 启动定时器
    lisa_timer_change_period(g_snooze_ctx.snooze_timer, g_snooze_ctx.snooze_interval * 60 * 1000);
    lisa_timer_start(g_snooze_ctx.snooze_timer);

    LISA_LOGI(TAG, "snooze timer started: interval=%dmin, will ring in %d minutes",
              g_snooze_ctx.snooze_interval, g_snooze_ctx.snooze_interval);
}

static void alarm_snooze_stop(void)
{
    lisa_timer_t *snooze_timer = g_snooze_ctx.snooze_timer;

    if (g_snooze_ctx.snooze_timer) {
        lisa_timer_stop(g_snooze_ctx.snooze_timer);
    }

    memset(&g_snooze_ctx, 0, sizeof(g_snooze_ctx));
    g_snooze_ctx.snooze_timer = snooze_timer;
    LISA_LOGI(TAG, "snooze stopped");
}

/* ==================== 触发处理内部函数 ==================== */

static void alarm_handle_next_trigger(const alarm_object_t *alarm_obj, uint64_t fired_ts, time_t now_ts)
{
    if (alarm_obj->trigger.type == ALARM_TRIG_ONCE) {
        // 单次闹钟，直接删除
        alarm_nvs_delete(alarm_obj->cloud_id);
        voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
        LISA_LOGI(TAG, "once alarm deleted, cloud_id=%llu",
                  (unsigned long long)alarm_obj->cloud_id);
    } else if (alarm_obj->trigger.type == ALARM_TRIG_CUSTOM) {
        // Custom 类型闹钟，通过 API 查询下次触发日期
        LISA_LOGI(TAG, "custom alarm auto-triggered, querying next date, cloud_id=%llu",
                  (unsigned long long)alarm_obj->cloud_id);

        char next_date[16] = {0};
        int api_ret = listenai_custom_alarm_next_date(alarm_obj->cloud_id, next_date, sizeof(next_date));
        if (api_ret != 0) {
            LISA_LOGE(TAG, "failed to query custom alarm next date, cloud_id=%llu, ret=%d",
                      (unsigned long long)alarm_obj->cloud_id, api_ret);
            // API 查询失败，直接删除闹钟
            alarm_nvs_delete(alarm_obj->cloud_id);
            voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
            return;
        }

        LISA_LOGI(TAG, "custom alarm next date: %s", next_date);

        // 解析日期
        uint16_t y = 0;
        uint8_t m = 0, d = 0;
        if (alarm_time_parse_ymd(next_date, &y, &m, &d) != 0) {
            LISA_LOGE(TAG, "failed to parse next date: %s", next_date);
            alarm_nvs_delete(alarm_obj->cloud_id);
            voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
            return;
        }

        // 创建新的下次闹钟
        alarm_object_t next_alarm = *alarm_obj;
        next_alarm.trigger.year = y;
        next_alarm.trigger.month = m;
        next_alarm.trigger.day = d;
        next_alarm.alarm_id = alarm_time_obj_to_timestamp(&next_alarm);

        // 删除旧实例
        alarm_nvs_delete(alarm_obj->cloud_id);

        if (next_alarm.alarm_id == 0) {
            LISA_LOGE(TAG, "invalid custom alarm next timestamp");
            voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
            return;
        }

        if (alarm_nvs_add(&next_alarm) == 0) {
            ls_alarm_insert_by_timestamp(next_alarm.alarm_id, next_alarm.cloud_id, next_alarm.text);
            LISA_LOGI(TAG, "custom alarm next_id=%llu created", (unsigned long long)next_alarm.alarm_id);
        } else {
            LISA_LOGE(TAG, "failed to create next custom alarm");
            voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
        }
    } else {
        // 循环闹钟，计算下次触发时间
        alarm_object_t next_alarm = *alarm_obj;
        uint64_t next_ts = alarm_calc_next_trigger(&next_alarm, now_ts);
        LISA_LOGI(TAG, "alarm next_ts=%llu (now=%llu)",
                  (unsigned long long)next_ts, (unsigned long long)now_ts);

        if (next_ts > now_ts) {
            // 删除旧实例
            alarm_nvs_delete(alarm_obj->cloud_id);

            // 创建新的下次闹钟
            next_alarm.alarm_id = next_ts;
            alarm_time_timestamp_to_date(next_alarm.alarm_id,
                                        &next_alarm.trigger.year,
                                        &next_alarm.trigger.month,
                                        &next_alarm.trigger.day,
                                        &next_alarm.trigger.hour,
                                        &next_alarm.trigger.minute,
                                        &next_alarm.trigger.second);

            if (alarm_nvs_add(&next_alarm) == 0) {
                ls_alarm_insert_by_timestamp(next_alarm.alarm_id, next_alarm.cloud_id, next_alarm.text);
                LISA_LOGI(TAG, "alarm next_id=%llu created", (unsigned long long)next_alarm.alarm_id);
            } else {
                LISA_LOGE(TAG, "failed to create next alarm, next_ts=%llu", (unsigned long long)next_ts);
            }
        } else {
            // 没有下次触发，直接删除
            alarm_nvs_delete(alarm_obj->cloud_id);
            voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
            LISA_LOGI(TAG, "no more triggers, alarm deleted");
        }
    }
}

/* ==================== 公开接口实现 ==================== */

static void alarm_process_next_handler(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_LOGI(TAG, "processing alarm next trigger (from message)");

    // 调用已有的处理函数
    alarm_handle_stop_and_next();
}

void alarm_handler_init(ls_alarm_user_callback_t user_callback)
{
    g_user_callback = user_callback;

    if (alarm_work_init() != 0) {
        LISA_LOGE(TAG, "alarm work init failed");
    }

    // 订阅消息，处理闹钟后续操作
    voice_msg_sub(VOICE_MSG_ALARM_PROCESS_NEXT, alarm_process_next_handler, NULL);
}

int alarm_process_triggered_async(uint64_t fired_ts, uint64_t cloud_id, int64_t now_ts)
{
    alarm_work_item_t item = {
        .type = ALARM_WORK_TRIGGERED,
        .fired_ts = fired_ts,
        .cloud_id = cloud_id,
        .now_ts = now_ts,
    };

    return alarm_submit_work(&item);
}

static void alarm_process_triggered_work(uint64_t fired_ts, uint64_t cloud_id, int64_t now_ts)
{
    uint64_t previous_snooze_cloud_id = 0;

    if (g_snooze_ctx.ringing_cloud_id != 0 &&
        g_snooze_ctx.ringing_cloud_id != cloud_id) {
        previous_snooze_cloud_id = g_snooze_ctx.ringing_cloud_id;
        LISA_LOGI(TAG,
                  "new alarm cloud_id=%llu triggered while previous snooze cloud_id=%llu is pending, active=%d",
                  (unsigned long long)cloud_id,
                  (unsigned long long)previous_snooze_cloud_id,
                  alarm_ring_is_active());

        /* 第二个闹钟触发后，第一个闹钟不再保留 snooze 待处理状态；
         * 它会在本次触发完成后直接推进到下一次触发。 */
        alarm_snooze_stop();
    }

    // 查询闹钟对象
    const alarm_object_t *alarm_obj_ptr = alarm_nvs_find(cloud_id);
    if (!alarm_obj_ptr) {
        LISA_LOGW(TAG, "alarm object not found, cloud_id=%llu", (unsigned long long)cloud_id);
        voice_msg_pub(VOICE_MSG_ALARM_DELETE, &fired_ts, sizeof(fired_ts));
        if (previous_snooze_cloud_id != 0) {
            alarm_submit_stop_and_next_work(previous_snooze_cloud_id, false);
        }
        return;
    }
    alarm_object_t alarm_obj = *alarm_obj_ptr;

    LISA_LOGI(TAG, "alarm fired, cloud_id=%llu type=%u cal=%u snooze=%d",
              (unsigned long long)cloud_id,
              (unsigned)alarm_obj.trigger.type,
              (unsigned)alarm_obj.calendar,
              alarm_obj.snooze_enabled);

    // 检查是否开启 snooze
    if (alarm_obj.snooze_enabled && alarm_obj.snooze_count > 0) {
        // 准备 snooze 模式（不启动定时器，等待用户按键）
        alarm_snooze_prepare(cloud_id, alarm_obj.snooze_interval, alarm_obj.snooze_count);
        LISA_LOGI(TAG, "snooze prepared, waiting for user action");
    } else {
        // 未开启 snooze，处理下次触发
        alarm_handle_next_trigger(&alarm_obj, fired_ts, (time_t)now_ts);
    }

    // 通知上层响铃
    if (g_user_callback) {
        g_user_callback(fired_ts, (const uint8_t *)alarm_obj.text);
    }

    if (previous_snooze_cloud_id != 0) {
        alarm_submit_stop_and_next_work(previous_snooze_cloud_id, false);
    }
}

void alarm_handle_snooze(void)
{
    // 检查是否有snooze配置
    if (g_snooze_ctx.ringing_cloud_id == 0) {
        LISA_LOGW(TAG, "snooze clicked but no alarm configured");
        alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_DELETE);
        return;
    }

    // 检查是否还能继续 snooze
    if (g_snooze_ctx.snooze_current_count >= g_snooze_ctx.snooze_max_count) {
        LISA_LOGI(TAG, "snooze reached max count, stopping and advancing alarm");
        // 显示"闹钟关闭成功!"toast
        alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_DELETE);
        voice_msg_pub(VOICE_MSG_ALARM_PROCESS_NEXT, NULL, 0);
        return;
    }

    // 如果定时器还没启动，启动它
    if (!g_snooze_ctx.snooze_mode_active) {
        alarm_snooze_start_timer();
        LISA_LOGI(TAG, "user clicked snooze, timer started, will ring again in %dmin", g_snooze_ctx.snooze_interval);
    } else {
        // 定时器已经在运行，说明这是在snooze期间又按了一次
        LISA_LOGI(TAG, "user clicked snooze again while timer running");
    }

    alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_SNOOZE);

    // 当前响铃的停止由上层（alarm_ring）处理
}

void alarm_handle_stop_and_next(void)
{
    uint64_t cloud_id = g_snooze_ctx.ringing_cloud_id;
    if (alarm_submit_stop_and_next_work(cloud_id, true) != 0) {
        LISA_LOGE(TAG, "failed to submit stop-and-next alarm work");
    }
}

static void alarm_handle_stop_and_next_work(uint64_t cloud_id, bool publish_result)
{
    // 检查是否有snooze配置
    if (cloud_id == 0) {
        LISA_LOGW(TAG, "stop clicked but no alarm configured");
        if (publish_result) {
            alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_DELETE);
        }
        return;
    }

    LISA_LOGI(TAG, "stop alarm and process next, cloud_id=%llu",
              (unsigned long long)cloud_id);

    // 只清理当前 snooze 状态，避免异步处理旧闹钟时误清掉新触发闹钟的状态
    if (g_snooze_ctx.ringing_cloud_id == cloud_id) {
        alarm_snooze_stop();
    }

    // 查询闹钟对象并处理下一个触发时间
    const alarm_object_t *alarm_obj_ptr = alarm_nvs_find(cloud_id);
    if (!alarm_obj_ptr) {
        LISA_LOGW(TAG, "alarm object not found in NVS, cloud_id=%llu",
                  (unsigned long long)cloud_id);
        return;
    }
    alarm_object_t alarm_copy = *alarm_obj_ptr;
    const alarm_object_t *alarm_obj = &alarm_copy;

    LISA_LOGI(TAG, "found alarm object, type=%d, cloud_id=%llu",
              alarm_obj->trigger.type, (unsigned long long)cloud_id);

    time_t now_ts = (time_t)alarm_time_get_network_timestamp();

    if (alarm_obj->trigger.type == ALARM_TRIG_ONCE) {
        // 单次闹钟，直接删除
        LISA_LOGI(TAG, "deleting once alarm, cloud_id=%llu",
                  (unsigned long long)cloud_id);
        uint64_t alarm_id = alarm_obj->alarm_id;
        int ret = alarm_nvs_delete(cloud_id);
        if (ret == 0) {
            // 发送消息通知UI更新闹钟图标
            voice_msg_pub(VOICE_MSG_ALARM_DELETE, &alarm_id, sizeof(alarm_id));
            if (publish_result) {
                alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_DELETE);
            }
            LISA_LOGI(TAG, "once alarm stopped and deleted successfully, cloud_id=%llu",
                      (unsigned long long)cloud_id);
        } else {
            LISA_LOGE(TAG, "failed to delete once alarm, cloud_id=%llu, ret=%d",
                      (unsigned long long)cloud_id, ret);
        }
    } else if (alarm_obj->trigger.type == ALARM_TRIG_CUSTOM) {
        // Custom 类型闹钟，通过 API 查询下次触发日期
        LISA_LOGI(TAG, "custom alarm, querying next date from API, cloud_id=%llu",
                  (unsigned long long)cloud_id);

        char next_date[16] = {0};
        int api_ret = listenai_custom_alarm_next_date(cloud_id, next_date, sizeof(next_date));
        if (api_ret != 0) {
            LISA_LOGE(TAG, "failed to query custom alarm next date, cloud_id=%llu, ret=%d",
                      (unsigned long long)cloud_id, api_ret);
            return;
        }

        LISA_LOGI(TAG, "custom alarm next date: %s", next_date);

        // 解析日期
        uint16_t y = 0;
        uint8_t m = 0, d = 0;
        if (alarm_time_parse_ymd(next_date, &y, &m, &d) != 0) {
            LISA_LOGE(TAG, "failed to parse next date: %s", next_date);
            return;
        }

        // 创建新的下次闹钟
        alarm_object_t next_alarm = *alarm_obj;
        next_alarm.trigger.year = y;
        next_alarm.trigger.month = m;
        next_alarm.trigger.day = d;
        next_alarm.alarm_id = alarm_time_obj_to_timestamp(&next_alarm);

        // 删除旧实例
        LISA_LOGI(TAG, "deleting old custom alarm instance, cloud_id=%llu",
                  (unsigned long long)cloud_id);
        uint64_t old_alarm_id = alarm_obj->alarm_id;
        int ret = alarm_nvs_delete(cloud_id);
        if (ret != 0) {
            LISA_LOGE(TAG, "failed to delete old custom alarm, cloud_id=%llu, ret=%d",
                      (unsigned long long)cloud_id, ret);
            return;
        }

        if (next_alarm.alarm_id == 0) {
            LISA_LOGE(TAG, "invalid custom alarm next timestamp");
            return;
        }

        ret = alarm_nvs_add(&next_alarm);
        if (ret == 0) {
            ls_alarm_insert_by_timestamp(next_alarm.alarm_id, next_alarm.cloud_id, next_alarm.text);
            // 发送消息通知UI：删除旧闹钟
            voice_msg_pub(VOICE_MSG_ALARM_DELETE, &old_alarm_id, sizeof(old_alarm_id));
            // 发送消息通知UI：创建新闹钟
            // alarm_publish_create_msg(&next_alarm);
            if (publish_result) {
                alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_NEXT);
            }
            LISA_LOGI(TAG, "custom alarm next_id=%llu created successfully",
                      (unsigned long long)next_alarm.alarm_id);
        } else {
            LISA_LOGE(TAG, "failed to create next custom alarm, ret=%d", ret);
        }
    } else {
        // 循环闹钟，计算下次触发时间
        alarm_object_t next_alarm = *alarm_obj;
        uint64_t next_ts = alarm_calc_next_trigger(&next_alarm, now_ts);

        LISA_LOGI(TAG, "recurring alarm next_ts=%llu (now=%llu)",
                  (unsigned long long)next_ts, (unsigned long long)now_ts);

        if (next_ts > now_ts) {
            // 删除旧实例
            LISA_LOGI(TAG, "deleting old alarm instance, cloud_id=%llu",
                      (unsigned long long)cloud_id);
            uint64_t old_alarm_id = alarm_obj->alarm_id;
            int ret = alarm_nvs_delete(cloud_id);
            if (ret != 0) {
                LISA_LOGE(TAG, "failed to delete old alarm, cloud_id=%llu, ret=%d",
                          (unsigned long long)cloud_id, ret);
                return;
            }

            // 创建新的下次闹钟
            next_alarm.alarm_id = next_ts;
            alarm_time_timestamp_to_date(next_alarm.alarm_id,
                &next_alarm.trigger.year, &next_alarm.trigger.month, &next_alarm.trigger.day,
                &next_alarm.trigger.hour, &next_alarm.trigger.minute, &next_alarm.trigger.second);

            ret = alarm_nvs_add(&next_alarm);
            if (ret == 0) {
                ls_alarm_insert_by_timestamp(next_alarm.alarm_id, next_alarm.cloud_id, next_alarm.text);
                // 发送消息通知UI：删除旧闹钟
                voice_msg_pub(VOICE_MSG_ALARM_DELETE, &old_alarm_id, sizeof(old_alarm_id));
                // 发送消息通知UI：创建新闹钟
                // alarm_publish_create_msg(&next_alarm);
                if (publish_result) {
                    alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_NEXT);
                }
                LISA_LOGI(TAG, "alarm next_id=%llu created successfully", (unsigned long long)next_ts);
            } else {
                LISA_LOGE(TAG, "failed to create next alarm, next_ts=%llu, ret=%d",
                          (unsigned long long)next_ts, ret);
            }
        } else {
            // 没有下次触发，直接删除
            LISA_LOGI(TAG, "no more triggers, deleting alarm, cloud_id=%llu",
                      (unsigned long long)cloud_id);
            uint64_t alarm_id = alarm_obj->alarm_id;
            int ret = alarm_nvs_delete(cloud_id);
            if (ret == 0) {
                // 发送消息通知UI更新闹钟图标
                voice_msg_pub(VOICE_MSG_ALARM_DELETE, &alarm_id, sizeof(alarm_id));
                if (publish_result) {
                    alarm_publish_action_result(VOICE_MSG_ALARM_ACTION_RESULT_DELETE);
                }
                LISA_LOGI(TAG, "alarm deleted successfully (no more triggers), cloud_id=%llu",
                          (unsigned long long)cloud_id);
            } else {
                LISA_LOGE(TAG, "failed to delete alarm (no more triggers), cloud_id=%llu, ret=%d",
                          (unsigned long long)cloud_id, ret);
            }
        }
    }
}

bool alarm_has_active_alarms(void)
{
    // 检查是否有待触发的闹钟
    int count = ls_alarm_count_get();
    if (count > 0) {
        return true;
    }

    // 检查是否有 snooze 配置（等待用户操作或定时器运行中）
    if (g_snooze_ctx.ringing_cloud_id != 0) {
        return true;
    }

    return false;
}

void alarm_handler_cancel_snooze_by_cloud_id(uint64_t cloud_id)
{
    if (cloud_id == 0) {
        return;
    }

    if (g_snooze_ctx.ringing_cloud_id != cloud_id) {
        return;
    }

    LISA_LOGI(TAG, "alarm deleted while snooze active, cancel snooze for cloud_id=%llu",
              (unsigned long long)cloud_id);
    alarm_snooze_stop();
}

int alarm_handler_get_snooze_remaining_count(void)
{
    // 如果没有正在响铃的闹钟，返回-1
    if (g_snooze_ctx.ringing_cloud_id == 0) {
        return -1;
    }

    // 查询闹钟对象
    const alarm_object_t *alarm_obj = alarm_nvs_find(g_snooze_ctx.ringing_cloud_id);
    if (!alarm_obj) {
        return -1;
    }

    // 如果没有开启snooze，返回-1
    if (!alarm_obj->snooze_enabled || alarm_obj->snooze_count == 0) {
        return -1;
    }

    // 返回剩余次数
    return g_snooze_ctx.snooze_max_count - g_snooze_ctx.snooze_current_count;
}
