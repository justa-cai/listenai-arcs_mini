#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "acomp_wakeup.h"

#define TAG "wakeup_out"
#include "lisa_log.h"

#define WAKEUP_AUDIO_OUT_STREAM_CH_INDEX (1)
#define WAKEUP_AUDIO_OUT_STREAM_CH_CNAME "stream.tocloud"
#define WAKEUP_AUDIO_OUT_STREAM    (1)
#define WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO   (1)

static SemaphoreHandle_t rx_sem;

static void wakeup_stream_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    
    if (event & WAKEUP_CB_EVENT_STREAM_UPDATE) {
        /* 算法音频输出事件 */
#ifdef WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        if (rx_sem != NULL) {
            xSemaphoreGive(rx_sem);
        }
#endif
    }
}

static void wakeup_out_task(void *pvParameters)
{
    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;
    int ret;

    while (1) {

#ifdef WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        /* 自动触发模式：等待来自回调的信号量 */
        xSemaphoreTake(rx_sem, pdMS_TO_TICKS(50));
#else
        /* 手动触发模式：等待超时时间 */
        vTaskDelay(pdMS_TO_TICKS(5));
#endif
        /* 从 R2M 流中获取数据 */
        while (1) {
            buffer = acomp_wakeup_stream_rx_buffer_get(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX, &len, &desc_idx);
            if (buffer != NULL && len > 0) {

                LISA_LOGD(TAG, "acomp_wakeup_stream_rx_buffer_get ok");

                // process_output_audio(buffer, len);

                /* 释放缓冲区 */
                ret = acomp_wakeup_stream_rx_buffer_release(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX, desc_idx, len, buffer);
                if (ret != ACOMP_ERR_OK) {
                    LISA_LOGW(TAG, "Failed to release buffer: %d", ret);
                }
            } else {
                /* 没有更多数据，跳出内层循环 */
                break;
            }
        }
    }

    vTaskDelete(NULL);
}


int wakeup_audio_out_init(void)
{
    int ret;

    acomp_stream_chn_create_desc_t rx_desc = {
        .cname = WAKEUP_AUDIO_OUT_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = WAKEUP_AUDIO_OUT_STREAM_CH_INDEX,
        .buffer_size = ACOMP_WAKEUP_AUDIO_OUTPUT_MAX_LEN_ONCE_FRAME,
        .num_descs = 8,
#if WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        .kick_policy = 1,  /* 手动触发 */
#else
        .kick_policy = 0,  /* 手动触发 */
#endif
    };
#if WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
    rx_sem = xSemaphoreCreateBinary();
    if (rx_sem == NULL) {
        LISA_LOGE(TAG, "Failed to create rx semaphore");
    }
#endif

    acomp_wakeup_stream_ch_enable(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX,&rx_desc);

    acomp_wakeup_add_callback(WAKEUP_CB_EVENT_STREAM_UPDATE, wakeup_stream_handler, NULL);

    ret = xTaskCreate(wakeup_out_task, "wakeup_out_task", 4096, NULL, 9, NULL);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create wakeup_out_task task");
    }

    return 0;
}
