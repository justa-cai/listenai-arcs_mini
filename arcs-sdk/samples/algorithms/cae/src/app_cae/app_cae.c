#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "acomp_cae.h"

#include "cae_audio_in.h"
#include "cae_audio_out.h"

#define TAG "cae"
#include "lisa_log.h"

static void cae_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    (void)priv;

    if (event & CAE_CB_EVENT_ENGINE_ANGLE) {
        for (uint32_t i = 0; i < event_data_len / sizeof(short); i++) {
            short angle = *((short *)event_data + i);
            LISA_LOGI(TAG, "cae angle[%u]: %d", i, angle);
        }
    } else if (event & CAE_CB_EVENT_ENGINE_ERROR) {
        LISA_LOGE(TAG, "cae engine error, len:%u", event_data_len);
    } else {
        LISA_LOGW(TAG, "unknown cae event:0X%X", event);
    }
}

int app_cae_init(void)
{
    int ret;

    ret = acomp_cae_init();
    LISA_LOGI(TAG, "acomp_cae_init ret:%d", ret);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_cae_add_callback(CAE_CB_EVENT_ENGINE_ANGLE | CAE_CB_EVENT_ENGINE_ERROR,
                                 cae_event_handler, NULL);
    LISA_LOGI(TAG, "acomp_cae_add_callback ret:%d", ret);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_cae_prepare();
    LISA_LOGI(TAG, "acomp_cae_prepare ret:%d", ret);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_cae_start();
    LISA_LOGI(TAG, "acomp_cae_start ret:%d", ret);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = cae_audio_out_init();
    if (ret != 0) {
        return ret;
    }

    ret = cae_audio_in_init();
    if (ret != 0) {
        return ret;
    }

    return ret;
}
