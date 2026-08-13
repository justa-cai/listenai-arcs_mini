#include <stdint.h>

#include "acomp_palm.h"
#include "app_palm.h"

#define TAG "app_palm"
#include "lisa_log.h"

static void palm_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    (void)priv;

    if ((event & PALM_CB_EVENT_ENGINE_RLT) == 0U) {
        return;
    }

    if (event_data == NULL || event_data_len < sizeof(acomp_palm_result_info_t)) {
        LISA_LOGW(TAG, "invalid palm result: data=%p len=%u", event_data, event_data_len);
        return;
    }

    acomp_palm_result_info_t *info = (acomp_palm_result_info_t *)event_data;
    uint32_t result_cnt = info->results_cnt;

    if (result_cnt == 0U) {
        LISA_LOGI(TAG, "no palm detected");
        return;
    }

    uint32_t max_area_index = info->max_area_results_index;
    if (max_area_index >= result_cnt) {
        max_area_index = 0U;
    }

    acomp_palm_result_t *result = &info->results[max_area_index];
    LISA_LOGI(TAG,
              "detect palm cnt:%u\n"
              "max area result:\n"
              "index:%u\n"
              "palm_rect:[x:%d, y:%d, w:%d, h:%d]\n"
              "palm_score:%f\n"
              "align_cnt:%d\n"
              "feature_cnt:%d\n"
              "compare_cnt:%d\n"
              "compare_scores:[%f, %f]\n",
              result_cnt,
              max_area_index,
              result->palm_rect.x,
              result->palm_rect.y,
              result->palm_rect.w,
              result->palm_rect.h,
              result->palm_score,
              result->n_align_point,
              result->feature_cnt,
              result->compare_cnt,
              result->compare_scores[0],
              result->compare_scores[1]);
}

int app_palm_init(void)
{
    int ret;

    ret = acomp_palm_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_palm_init failed: %d", ret);
        return ret;
    }

    ret = acomp_palm_prepare();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_palm_prepare failed: %d", ret);
        return ret;
    }

    ret = acomp_palm_start();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_palm_start failed: %d", ret);
        return ret;
    }

    ret = acomp_palm_add_callback(PALM_CB_EVENT_ENGINE_RLT, palm_event_handler, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_palm_add_callback failed: %d", ret);
        return ret;
    }

    return 0;
}
