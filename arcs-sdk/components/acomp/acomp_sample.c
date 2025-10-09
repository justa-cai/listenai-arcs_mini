#include "acomp.h"
#include "wsp/acomp_wsp.h"

#define TAG "acomp_sample"
#include "lisa_log.h"

void wsp_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{

    if (event & WSP_CB_EVENT_ENGINE_RLT) {
        LISA_LOGI(TAG, "wsp result:%d,%s", event_data_len, (char *)event_data);
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_BEGIN) {
        LISA_LOGI(TAG, "wsp vad begin frame index:%d", *(uint32_t*)event_data);
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_END) {
        LISA_LOGI(TAG, "wsp vad end frame index:%d", *(uint32_t*)event_data);
    } else {
        LISA_LOGW(TAG, "unknown wsp event:0X%X", event);
    }
}

void test_acomp(void)
{
    uint32_t count = 0;
    int ret = 0;
    acomp_init();
    acomp_wsp_init();
    acomp_wsp_add_callback(WSP_CB_EVENT_ENGINE_RLT | WSP_CB_EVENT_ENGINE_VAD_BEGIN | WSP_CB_EVENT_ENGINE_VAD_END,
                           wsp_event_handler, NULL);
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
}
