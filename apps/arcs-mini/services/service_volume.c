#include <stdbool.h>
#include <stdint.h>

#define TAG "volume"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "lisa_log.h"
#include "lisa_kv.h"
#include "kv.h"
#include "service_volume.h"
#include "voice_player_comm.h"

#define DEFAULT_VOLUME 50
#define DEFAULT_MIN_VOLUME 10
#define MAX_MIN_VOLUME 10
#define VOLUME_APPLY_QUEUE_SIZE 1
#define VOLUME_APPLY_TASK_STACK_SIZE 2048
#define VOLUME_APPLY_TASK_PRIORITY 5

static QueueHandle_t s_volume_apply_queue = NULL;

static int service_volume_min_clamp(int min_volume)
{
    if (min_volume < 0) {
        min_volume = 0;
    }
    if (min_volume > MAX_MIN_VOLUME) {
        min_volume = MAX_MIN_VOLUME;
    }

    return min_volume;
}

static int service_volume_get_min(void)
{
    int min_volume = DEFAULT_MIN_VOLUME;

    if (lisa_kv_get_int(KV_KEY_USER_MIN_VOLUME, &min_volume) != 0) {
        min_volume = DEFAULT_MIN_VOLUME;
    }

    return service_volume_min_clamp(min_volume);
}

static int service_volume_clamp(int volume)
{
    int min_volume = service_volume_get_min();

    if (volume < min_volume) {
        volume = min_volume;
    }
    if (volume > 100) {
        volume = 100;
    }

    return volume;
}

static void service_volume_apply(int volume)
{
    int failed_count = 0;

    volume = service_volume_clamp(volume);

    if (tone_player != NULL && app_player_set_volume(tone_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (tts_player != NULL && app_player_set_volume(tts_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (music_player != NULL && app_player_set_volume(music_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (alert_player != NULL && app_player_set_volume(alert_player, volume) != APP_PLAYER_OK) {
        failed_count++;
    }

    if (failed_count > 0) {
        LOGW("Failed to apply volume for %d player(s)", failed_count);
    }
}

static void service_volume_apply_task_entry(void *pvParameters)
{
    int volume = DEFAULT_VOLUME;

    while (1) {
        if (xQueueReceive(s_volume_apply_queue, &volume, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        LOGI("Applying volume asynchronously: %d", volume);
        service_volume_apply(volume);
    }
}

static int service_volume_async_init(void)
{
    if (s_volume_apply_queue != NULL) {
        return 0;
    }

    s_volume_apply_queue = xQueueCreate(VOLUME_APPLY_QUEUE_SIZE, sizeof(int));
    if (s_volume_apply_queue == NULL) {
        LOGE("failed to create volume apply queue");
        return -1;
    }

    BaseType_t ret = xTaskCreate(
        service_volume_apply_task_entry,
        "volume_apply",
        VOLUME_APPLY_TASK_STACK_SIZE,
        NULL,
        VOLUME_APPLY_TASK_PRIORITY,
        NULL
    );
    if (ret != pdPASS) {
        LOGE("failed to create volume apply task");
        vQueueDelete(s_volume_apply_queue);
        s_volume_apply_queue = NULL;
        return -1;
    }

    return 0;
}

static void service_volume_apply_async(int volume)
{
    volume = service_volume_clamp(volume);

    if (s_volume_apply_queue == NULL) {
        LOGW("volume apply queue not ready, fallback to sync apply");
        service_volume_apply(volume);
        return;
    }

    if (xQueueOverwrite(s_volume_apply_queue, &volume) != pdPASS) {
        LOGW("failed to enqueue volume apply, fallback to sync apply");
        service_volume_apply(volume);
    }
}

void service_volume_init(void)
{
    if (service_volume_async_init() != 0) {
        LOGW("volume async apply init failed, keep sync mode");
    }

    service_volume_restore_from_kv();

    LOGI("Volume service initialized, min volume: %d", service_volume_get_min());
}

void service_volume_set(int volume)
{
    volume = service_volume_clamp(volume);

    lisa_kv_set_int(KV_KEY_USER_VOLUME, volume);
    service_volume_apply_async(volume);

    LOGI("Volume set to %d", volume);
}

void service_volume_set_temp(int volume)
{
    volume = service_volume_clamp(volume);
    service_volume_apply(volume);

    LOGI("Volume temporarily set to %d", volume);
}

void service_volume_restore_from_kv(void)
{
    int volume = service_volume_get();

    service_volume_apply(volume);
    LOGI("Volume restored from kv to %d", volume);
}

int service_volume_get(void)
{
    int volume = DEFAULT_VOLUME;

    if (lisa_kv_get_int(KV_KEY_USER_VOLUME, &volume) != 0) {
        volume = DEFAULT_VOLUME;
    }

    return service_volume_clamp(volume);
}

void service_volume_adjust(int delta)
{
    int current_vol = service_volume_get();
    int target_vol = current_vol + delta;

    service_volume_set(target_vol);
}
