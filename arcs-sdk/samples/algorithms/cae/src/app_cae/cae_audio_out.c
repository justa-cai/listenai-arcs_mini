#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "acomp_cae.h"

#include "cae_uac.h"

#define TAG "cae_out"
#include "lisa_log.h"

#define CAE_AUDIO_OUT_STREAM_CH_INDEX (1)
#define CAE_AUDIO_OUT_STREAM_CH_CNAME "stream.tocloud"
#define CAE_AUDIO_OUT_STREAM_KICK_AUTO (1)

static SemaphoreHandle_t rx_sem;

static void cae_stream_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    (void)event_data;
    (void)event_data_len;
    (void)priv;

    if (event & CAE_CB_EVENT_STREAM_UPDATE) {
#if CAE_AUDIO_OUT_STREAM_KICK_AUTO
        if (rx_sem != NULL) {
            xSemaphoreGive(rx_sem);
        }
#endif
    }
}

static void cae_out_task(void *pvParameters)
{
    (void)pvParameters;

    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;
    int ret;

    while (1) {
#if CAE_AUDIO_OUT_STREAM_KICK_AUTO
        xSemaphoreTake(rx_sem, pdMS_TO_TICKS(50));
#else
        vTaskDelay(pdMS_TO_TICKS(5));
#endif
        while (1) {
            buffer = acomp_cae_stream_rx_buffer_get(CAE_AUDIO_OUT_STREAM_CH_INDEX, &len, &desc_idx);
            if (buffer != NULL && len > 0) {
                LISA_LOGD(TAG, "acomp_cae_stream_rx_buffer_get ok, len:%u", len);

                cae_uac_write(buffer, len);
                ret = acomp_cae_stream_rx_buffer_release(CAE_AUDIO_OUT_STREAM_CH_INDEX, desc_idx, len, buffer);
                if (ret != ACOMP_ERR_OK) {
                    LISA_LOGW(TAG, "Failed to release buffer: %d", ret);
                }
            } else {
                break;
            }
        }
    }

    vTaskDelete(NULL);
}

int cae_audio_out_init(void)
{
    int ret;

    ret = cae_uac_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "cae_uac_init failed: %d", ret);
        return ret;
    }

    acomp_stream_chn_create_desc_t rx_desc = {
        .cname = CAE_AUDIO_OUT_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = CAE_AUDIO_OUT_STREAM_CH_INDEX,
        .buffer_size = ACOMP_CAE_AUDIO_OUTPUT_MAX_LEN_ONCE_FRAME,
        .num_descs = 8,
#if CAE_AUDIO_OUT_STREAM_KICK_AUTO
        .kick_policy = 1,
#else
        .kick_policy = 0,
#endif
    };

#if CAE_AUDIO_OUT_STREAM_KICK_AUTO
    rx_sem = xSemaphoreCreateBinary();
    if (rx_sem == NULL) {
        LISA_LOGE(TAG, "Failed to create rx semaphore");
        return -1;
    }
#endif

    ret = acomp_cae_stream_ch_enable(CAE_AUDIO_OUT_STREAM_CH_INDEX, &rx_desc);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp_cae_stream_ch_enable failed: %d", ret);
        return ret;
    }

    ret = acomp_cae_add_callback(CAE_CB_EVENT_STREAM_UPDATE, cae_stream_handler, NULL);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp_cae_add_callback failed: %d", ret);
        return ret;
    }

    ret = xTaskCreate(cae_out_task, "cae_out_task", 4096, NULL, 9, NULL);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create cae_out_task task");
        return -1;
    }

    return 0;
}
