#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "acomp.h"
#include "wsp/acomp_wsp.h"
#include "FreeRTOS.h"
#define TAG "acomp_sample"
#include "lisa_log.h"



void wsp_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{

    if (event & WSP_CB_EVENT_ENGINE_RLT) {
        comp_wsp_result_t *result = (comp_wsp_result_t *)event_data;
        LISA_LOGI(TAG, "wsp result:%d,%s", result->len, result->data);
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_BEGIN) {
        LISA_LOGI(TAG, "wsp vad begin frame index:%d", *(uint32_t*)event_data);
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_END) {
        LISA_LOGI(TAG, "wsp vad end frame index:%d", *(uint32_t*)event_data);
    } 
    else if(event & WSP_CB_EVENT_STREAM_UPDATE){
        acomp_stream_channel_t *chn = event_data;
        LISA_LOGI(TAG, "wsp stream channel %d update", chn->idx);
    }
    else {
        LISA_LOGW(TAG, "unknown wsp event:0X%X", event);
    }
}

void test_acomp(void)
{
    uint32_t count = 0;
    int ret = 0;
    acomp_stream_chn_create_desc_t desc = {
        .cname = "wsp.stream.0",
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = 0,
        .buffer_size = 320,
        .num_descs = 128,
        .kick_policy = 0,
    };
    LISA_LOGI(TAG, "acomp_init");
    acomp_init();
    LISA_LOGI(TAG, "acomp_wsp_init");
    acomp_wsp_init();
    LISA_LOGI(TAG, "acomp_wsp_add_callback");
    acomp_wsp_add_callback(WSP_CB_EVENT_ENGINE_RLT | WSP_CB_EVENT_ENGINE_VAD_BEGIN | WSP_CB_EVENT_ENGINE_VAD_END | WSP_CB_EVENT_STREAM_UPDATE,
                         wsp_event_handler, NULL);
#if 0
    do {
        ret = acomp_wsp_prepare();
        LISA_LOGI(TAG, "acomp_wsp_prepare ret:%d", ret);
        ret = acomp_wsp_start();
        LISA_LOGI(TAG, "acomp_wsp_start ret:%d", ret);
        vTaskDelay(1000);
        LISA_LOGI(TAG, "goto stop");
        ret = acomp_wsp_stop();
        LISA_LOGI(TAG, "acomp_wsp_stop ret:%d", ret);
        ret = acomp_wsp_cleanup();
        LISA_LOGI(TAG, "acomp_wsp_cleanup ret:%d", ret);
        LISA_LOGI(TAG, "\n\n==========test_acomp run count:%d==========\n\n", count++);
        vTaskDelay(3000);

    } while (0);
#endif

    do{
        ret = acomp_wsp_stream_ch_enable(0,&desc);
         LISA_LOGI(TAG, "acomp_wsp_stream_ch_enable %s ret:%d", desc.cname, ret);
        ret = acomp_wsp_prepare();
        LISA_LOGI(TAG, "acomp_wsp_prepare ret:%d", ret);
        ret = acomp_wsp_start();
        LISA_LOGI(TAG, "acomp_wsp_start ret:%d", ret);
        uint64_t start_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        uint64_t current_ms;
        int fd = open("/SD:/wsp_audio.pcm", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            LISA_LOGE(TAG, "Failed to open file /SD:/wsp_audio.pcm");
            goto cleanup;
        }
        LISA_LOGI(TAG, "Audio file opened successfully, fd=%d", fd);
        for(;;){
    
            uint8_t* audio_buffer;
            uint32_t len;
            uint32_t desc_idx;
            int written;

            audio_buffer = acomp_wsp_stream_rx_buffer_get(0, &len, &desc_idx);

            if(audio_buffer == NULL){
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            LISA_LOGI(TAG,"acomp_wsp stream %s get audio buffer:%p,len:%d,desc_idx:%d",desc.cname,audio_buffer,len,desc_idx);
            LISA_LOGH(TAG,audio_buffer,64,"audio_buffer:");
            written = write(fd, audio_buffer, len);
            if (written != (int)len) {
                LISA_LOGE(TAG, "Write failed: expected %d bytes, wrote %d bytes", len, written);
            }

            acomp_wsp_stream_rx_buffer_release(0,desc_idx,len,audio_buffer);
            current_ms = pdTICKS_TO_MS(xTaskGetTickCount());
            if(current_ms - start_ms > 3000){
                break;
            }
        }
        close(fd);
cleanup:
        LISA_LOGI(TAG, "goto stop");
        ret = acomp_wsp_stop();
        LISA_LOGI(TAG, "acomp_wsp_stop ret:%d", ret);
        ret = acomp_wsp_cleanup();
        LISA_LOGI(TAG, "acomp_wsp_cleanup ret:%d", ret);
        ret = acomp_wsp_stream_ch_disable(0);
        LISA_LOGI(TAG, "acomp_wsp_stream_ch_disable ret:%d", ret);
    }while(0);
}
