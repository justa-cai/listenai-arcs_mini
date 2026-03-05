#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"
#include "acomp_wakeup.h"

#include "wakeup_audio_in.h"
#include "wakeup_audio_out.h"

#define TAG "wakeup"
#include "lisa_log.h"


static int app_algo_keyword_and_kid_extract(const uint8_t *const in)
{
	int ret = -1;
	int len = strlen(in);

	cJSON *root = NULL;
	if ((root = cJSON_Parse(in))) {
		cJSON	  *item	= cJSON_GetObjectItem(root, "rlt")->child;
		uint16_t wkkey	= cJSON_GetObjectItem(item, "iresid")->valueint;
		uint16_t wkncm	= cJSON_GetObjectItem(item, "ncm")->valueint;
		char	 *wkcmd	= cJSON_GetObjectItem(item, "keyword")->valuestring;

        int wkmain = -1;
		if (cJSON_HasObjectItem(item, "bMain")) {
			wkmain = cJSON_GetObjectItem(item, "bMain")->valueint;
		}

        LISA_LOGI(TAG, "WAKE(%d): KEY=%d(%s), NCM=%d, len:%d", wkmain, wkkey, wkcmd, wkncm, len);

		cJSON_Delete(root);
	} else {
		LISA_LOGE(TAG, "JSON:\"%s\"", in);
	}

	return ret;
}

static void wakeup_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    int ret;

    if (event & WAKEUP_CB_EVENT_ENGINE_RLT) {
        /* 算法识别结果 */
        LISA_LOGD(TAG, "wakeup result:%d,%s", event_data_len, (char *)event_data);
        app_algo_keyword_and_kid_extract((const uint8_t *)event_data);
    } else if (event & WAKEUP_CB_EVENT_ENGINE_TIMEOUT) {
        /* 算法唤醒超时 */
        LISA_LOGI(TAG, "wakeup timeout");

    } else if (event & WAKEUP_CB_EVENT_ENGINE_ANGLE) {
        /* 算法识别角度 */
        for (int i = 0; i < event_data_len/sizeof(short); i++) {
            short angle = *((short*)event_data + i);
            LISA_LOGD(TAG, "wakeup angle[%d]: %d", i, angle);
        }
    } else {
        LISA_LOGW(TAG, "unknown wakeup event:0X%X", event);
    }
}

int app_wakeup_init(void)
{
    int ret = 0;

    acomp_wakeup_init();
    acomp_wakeup_add_callback(WAKEUP_CB_EVENT_ENGINE_RLT 
                                | WAKEUP_CB_EVENT_ENGINE_TIMEOUT 
                                | WAKEUP_CB_EVENT_ENGINE_ANGLE,
                           wakeup_event_handler, NULL);

    ret = acomp_wakeup_prepare();
    LISA_LOGI(TAG, "acomp_wakeup_prepare ret:%d", ret);
    ret = acomp_wakeup_start();
    LISA_LOGI(TAG, "acomp_wakeup_start ret:%d", ret);

    ret = acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP);
    LISA_LOGI(TAG, "acomp_wakeup_set_algo_mode ret:%d", ret);

    wakeup_audio_out_init();
    wakeup_audio_in_init();

    return ret;
}
