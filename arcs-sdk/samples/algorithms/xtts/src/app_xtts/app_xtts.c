#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "xtts/acomp_xtts.h"
#include "resmgr/resmgr.h"

#include "xtts_audio_out.h"

#define TAG "app_xtts"
#include "lisa_log.h"

#define XTTS_STATUS_PCM_BEGIN   (1UL << 0)
#define XTTS_STATUS_PCM_END     (1UL << 2)
#define XTTS_STATUS_PCM_STOP    (1UL << 3)

#define XTTS_ROLE_DEFAULT       0
#define XTTS_SPEED_DEFAULT      50
#define XTTS_VOLUME_DEFAULT     50
#define XTTS_WAIT_TIMEOUT_MS    120000

static SemaphoreHandle_t g_xtts_rx_sem;
static SemaphoreHandle_t g_xtts_done_sem;
static volatile bool g_xtts_finished;
static volatile bool g_xtts_done_notified;

static void xtts_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    if ((event & XTTS_CB_EVENT_STATUS) && event_data != NULL && event_data_len >= sizeof(uint32_t)) {
        uint32_t status = *(uint32_t *)event_data;

        if (status == XTTS_STATUS_PCM_BEGIN) {
            LISA_LOGI(TAG, "xtts pcm begin");
        } else if (status == XTTS_STATUS_PCM_END) {
            LISA_LOGI(TAG, "xtts pcm end");
            g_xtts_finished = true;
        } else if (status == XTTS_STATUS_PCM_STOP) {
            LISA_LOGI(TAG, "xtts pcm stop");
            g_xtts_finished = true;
        } else {
            LISA_LOGI(TAG, "xtts status: 0x%x", status);
        }

        if (g_xtts_rx_sem != NULL) {
            xSemaphoreGive(g_xtts_rx_sem);
        }
    }

    if ((event & XTTS_CB_EVENT_STREAM_UPDATE) && g_xtts_rx_sem != NULL) {
        xSemaphoreGive(g_xtts_rx_sem);
    }
}

static void xtts_play_task(void *arg)
{
    while (1) {
        uint32_t len;
        uint16_t desc_idx;
        bool drained = true;

        xSemaphoreTake(g_xtts_rx_sem, pdMS_TO_TICKS(50));

        while (1) {
            uint8_t *buffer = acomp_xtts_stream_rx_buffer_get(0, &len, &desc_idx);
            if ((buffer == NULL) || (len == 0)) {
                break;
            }

            drained = false;
            xtts_audio_out_write(buffer, len);
            acomp_xtts_stream_rx_buffer_release(0, desc_idx, len, buffer);
        }

        if (g_xtts_finished && drained && !g_xtts_done_notified) {
            g_xtts_done_notified = true;
            xSemaphoreGive(g_xtts_done_sem);
            break;
        }
    }

    vTaskDelete(NULL);
}

int app_xtts_init(void)
{
    int ret;
    TaskHandle_t play_task = NULL;
    static const char tts_text[] = "Hello from XTTS sample running on the CP core.";

    g_xtts_rx_sem = xSemaphoreCreateBinary();
    g_xtts_done_sem = xSemaphoreCreateBinary();
    if ((g_xtts_rx_sem == NULL) || (g_xtts_done_sem == NULL)) {
        LISA_LOGE(TAG, "create xtts semaphore failed");
        return -1;
    }

#ifdef CONFIG_ACOMP_RESMGR
    ret = resmgr_init(CONFIG_ACOMP_RESMGR_FLASH_ADDR);
    if (ret != 0) {
        LISA_LOGW(TAG, "resmgr_init failed: %d", ret);
    }
#endif

    ret = xtts_audio_out_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "xtts_audio_out_init failed: %d", ret);
        return ret;
    }

    g_xtts_finished = false;
    g_xtts_done_notified = false;

    ret = xTaskCreate(xtts_play_task, "xtts_play", 4096, NULL, 8, &play_task);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "create xtts_play task failed");
        return -1;
    }

    ret = acomp_xtts_do_prepare(xtts_event_handler, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_xtts_do_prepare failed: %d", ret);
        return ret;
    }

    acomp_xtts_set_speed(XTTS_SPEED_DEFAULT);
    acomp_xtts_set_volume(XTTS_VOLUME_DEFAULT);
    acomp_xtts_set_role(XTTS_ROLE_DEFAULT);

    ret = acomp_xtts_synth_text(tts_text, strlen(tts_text));
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_xtts_synth_text failed: %d", ret);
        acomp_xtts_do_cleanup();
        xtts_audio_out_stop();
        return ret;
    }

    if (xSemaphoreTake(g_xtts_done_sem, pdMS_TO_TICKS(XTTS_WAIT_TIMEOUT_MS)) != pdTRUE) {
        LISA_LOGE(TAG, "wait xtts finish timeout");
        acomp_xtts_do_cleanup();
        xtts_audio_out_stop();
        return -1;
    }

    xtts_audio_out_stop();
    ret = acomp_xtts_do_cleanup();
    LISA_LOGI(TAG, "xtts sample finished: %d", ret);

    return ret;
}
